/*

 */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _5X6_DECOD_H_
#define _5X6_DECOD_H_

/* Includes ------------------------------------------------------------------*/
#include <adios.h>

/////////////////////////////////////////////////////////////////////////////
// HOW THE HOST DISPLAY BUS IS READ
//
// Both instruments drive their LCD with a NEC uPD7225 controller: 32 SEG x
// 4 COM = 128 cells, fed over a three-wire serial bus. This module listens
// in on that bus and never drives it.
//   CS   (EXTI line 13) frames a transfer; its falling edge also RESETS the
//                       data pointer of the controller to 0
//   CLK  (EXTI line 15) clocks one bit per rising edge, MSB FIRST
//   MOSI                the bit itself
//
// WHAT THE BYTES ARE
// Every byte is a uPD7225 instruction: 4 opcode bits then 4 data bits. Only
// three of its nineteen instructions ever appear on this bus:
//   1101 dddd   WRITE DATA MEMORY           -> 0xDx, the segment plane
//   1100 dddd   WRITE BLINKING DATA MEMORY  -> 0xCx, the blink plane
//   111d dddd   LOAD DATA POINTER           -> 0xF2 = point at SEG 18
// A write lands its 4 data bits on the 4 COM lines of the SEG address held
// by the data pointer, then increments it. So the Nth write of a burst
// addresses SEG N - which is why the array index below IS the SEG address.
// The planes are bit-parallel: blinking bit (SEG s, COM c) marks data bit
// (SEG s, COM c) as blinking.
//
// FRAMING - this is where the two machines part
//
//   TR-505 : ONE burst. CS falls (pointer = 0), 32 WRITE DATA MEMORY bytes
//            arrive, CS rises. segments[n] = SEG n. Decode fires on the
//            32nd byte.
//
//   TR-626 : TWO bursts, and a frame is only valid as a pair.
//            burst 1 - 15 bytes: buff[0] = 0xF2 (pointer -> 18), then 14
//                      WRITE BLINKING bytes, so buff[1..14] = SEG 18..31
//            burst 2 - 32 bytes: pointer back to 0, buff[15..46] = SEG 0..31
//            Validated on the CS RISING edge of the second burst - an event
//            the 505 does not raise at all. Then:
//                blinks[k]   = buff[k]     -> SEG 17+k  for k in 1..14
//                segments[n] = buff[15+n]  -> SEG n
//            The blink plane marks an instrument BLOCKED - the second layer
//            of the 30-instrument set. The 505 has neither.
//
// SEVEN-SEGMENT PACKING (both machines, same driver, same layout)
// A digit spans two SEG addresses - the even one carries a/f/g/e on COM0-3,
// the odd one b/c/d on COM1-3 - packed into one byte as:
//     bit 0=b  1=c  2=d  3=a  4=f  5=g  6=e
// digits[0] is 7seg #1, the LEFTMOST on the panel.
//
// PROVENANCE - read this before trusting the tables
// The TR-626 map was cross-checked cell by cell against the official COM/SEG
// table in the TR-626 service notes, page 14 (LCD LDB9171A): all 128 cells
// agree. THERE IS NO SUCH TABLE FOR THE TR-505 - its map was worked out by
// reverse engineering the running instrument, and this header is its only
// record. The segment packing above is taken from the 626 manual and applies
// to the 505 by construction, not by document.
//
// Notation: `digit3.5` = bit 5 of digit 3; `sel.12` = instrument 13.
// `.` = the host drives this cell to 1 permanently - a real segment, but a
//       constant one, so nothing decodes it.
// `---` = no segment wired there at all (the notation of the manual).
// Step and dot bit N is step N+1; instrument bit N is instrument N+1.
/////////////////////////////////////////////////////////////////////////////

/* --- TR-626 : SEGMENT plane, second burst, SEG 0..31 --------------------
   SEG  COM0          COM1          COM2          COM3
   [ 0] digit0.3(a)   digit0.4(f)   digit0.5(g)   digit0.6(e)
   [ 1] lbl.card      digit0.0(b)   digit0.1(c)   digit0.2(d)
   [ 2] digit1.3      digit1.4      digit1.5      digit1.6
   [ 3] lbl.tempo     digit1.0      digit1.1      digit1.2
   [ 4] digit2.3      digit2.4      digit2.5      digit2.6
   [ 5] lbl.measure   digit2.0      digit2.1      digit2.2
   [ 6] digit3.3      digit3.4      digit3.5      digit3.6
   [ 7] lbl.pitch     digit3.0      digit3.1      digit3.2
   [ 8] lbl.flam      ---           ---           lbl.shuffle
   [ 9] digit4.3      digit4.4      digit4.5      digit4.6
   [10] lbl.level     digit4.0      digit4.1      digit4.2
   [11] lbl.chain     group.0 (A)   group.1 (B)   group.2 (C)
   [12] .             group.3 (D)   group.4 (E)   group.5 (F)
   [13] digit5.3      digit5.4      digit5.5      digit5.6
   [14] lbl.block     digit5.0      digit5.1      digit5.2
   [15] digit6.3      digit6.4      digit6.5      digit6.6
   [16] mode.1        digit6.0      digit6.1      digit6.2
   [17] mode.2        mode.3        mode.4        lbl.sync
   [18] mode.0        sel.13        sel.14        sel.15
   [19] sel.4         sel.5         sel.6         sel.7
   [20] last.8        last.10       last.12       last.14
   [21] last.9        last.11       last.13       last.15
   [22] dots.8        dots.10       dots.12       dots.14
   [23] dots.9        dots.11       dots.13       dots.15
   [24] scale.0       scale.1       scale.2       scale.3
   [25] dots.6        dots.4        dots.2        dots.0
   [26] dots.7        dots.5        dots.3        dots.1
   [27] last.6        last.4        last.2        last.0
   [28] last.7        last.5        last.3        last.1
   [29] sel.3         sel.2         sel.1         sel.0
   [30] sel.11        sel.10        sel.9         sel.8
   [31] sel.12        .             lbl.accent    .
   The lower halves (SEG 25-28) run BACKWARDS - steps and dots 1-8 arrive
   COM0->COM3 descending. Panel wiring, confirmed by the manual.
   Cross-checked cell by cell against the service notes, page 14: the
   official table also names what each cell drives on the panel, and
   carries mode/label names this list does not repeat.
   ------------------------------------------------------------------- */

/* --- TR-626 : BLINK plane, first burst, 14 bytes at SEG 18..31 -----------
   The SAME cells as the segment plane above, seen on the other plane -
   which is why the instrument bits line up exactly.
   SEG  idx    COM0        COM1        COM2        COM3
   [18] b[ 1]  .           blk.13      blk.14      blk.15
   [19] b[ 2]  blk.4       blk.5       blk.6       blk.7
   [29] b[12]  blk.3       blk.2       blk.1       blk.0
   [30] b[13]  blk.11      blk.10      blk.9       blk.8
   [31] b[14]  blk.12      .           .           .
   SEG 20-28 (b[3..11]) carry no blinking instrument.
   ------------------------------------------------------------------- */

/* --- TR-505 : SEGMENT plane, 32 bytes, one burst ------------------------
   NO OFFICIAL TABLE EXISTS FOR THIS MACHINE. Reverse engineered.
   SEG  COM0          COM1          COM2          COM3
   [ 0] digit0.3(a)   digit0.4(f)   digit0.5(g)   digit0.6(e)
   [ 1] lbl.tempo     digit0.0(b)   digit0.1(c)   digit0.2(d)
   [ 2] digit1.3      digit1.4      digit1.5      digit1.6
   [ 3] lbl.measure   digit1.0      digit1.1      digit1.2
   [ 4] digit2.3      digit2.4      digit2.5      digit2.6
   [ 5] lbl.sync      digit2.0      digit2.1      digit2.2
   [ 6] digit3.3      digit3.4      digit3.5      digit3.6
   [ 7] lbl.grp_pat   digit3.0      digit3.1      digit3.2
   [ 8] lbl.chain     group.0       group.1       group.2
   [ 9] lbl.level     group.3       group.4       group.5
   [10] digit4.3      digit4.4      digit4.5      digit4.6
   [11] lbl.block     digit4.0      digit4.1      digit4.2
   [12] digit5.3      digit5.4      digit5.5      digit5.6
   [13] mode.0        digit5.0      digit5.1      digit5.2
   [14] mode.1        mode.2        mode.3        mode.4
   [15] .             sel.13        sel.14        sel.15
   [16] sel.4         sel.5         sel.6         sel.7
   [17] last.8        last.10       last.12       last.14
   [18] last.9        last.11       last.13       last.15
   [19] dots.8        dots.10       dots.12       dots.14
   [20] dots.9        dots.11       dots.13       dots.15
   [21] scale.0       scale.1       scale.2       scale.3
   [22] dots.6        dots.4        dots.2        dots.0
   [23] dots.7        dots.5        dots.3        dots.1
   [24] last.6        last.4        last.2        last.0
   [25] last.7        last.5        last.3        last.1
   [26] sel.3         sel.2         sel.1         sel.0
   [27] sel.11        sel.10        sel.9         sel.8
   [28] sel.12        acc.0         acc.1         .
   [29] .             .             .             .
   [30] .             .             .             .
   [31] .             .             .             .
   Six digits instead of seven, and ACCENT is a two-bit state here (acc.0,
   acc.1 at SEG28) where the 626 has a whole blink plane instead.
   Same backwards lower halves, SEG 22-27.
   SEG 29-31 are not read by the decoder - whether the host writes them at
   all was never checked.
   ------------------------------------------------------------------- */

/* Structures ----------------------------------------------------------------*/
typedef union {
  struct {
    u16 ALL;
  };
  struct {
    u16 measure:1;
    u16 tempo:1;
    u16 pitch:1; // adios_midi_event_t
    u16 level:1;
    u16 shuffle:1;
    u16 flam:1;
    u16 accent:1;
    u16 grp_pat:1; // adios_midi_chn_t
    u16 chain:1;
    u16 block:1;
    u16 sync:1;
    u16 card:1;
    u16 dummy:4;
  };
} tr5x6_decod_labels_t;

typedef union {
  struct {
    u8 ALL;
  };
  struct {	// in this order!
    u8 inc:1;
    u8 inst:1;
    u8 dec:1;
    u8 last:1;
    u8 dummy:4;
  };
} tr5x6_decod_buttons_t;

/* Prototypes ----------------------------------------------------------------*/
extern void TR5X6_DECOD_Init();
extern tr5x6_decod_buttons_t TR5X6_DECOD_BUTT_Handler(void);
// The LCD bus callback is a POINTER, aimed at boot by TR5X6_DECOD_Init().
// The two hosts do not even raise the same events - the 626 has a third
// one, on the CS rising edge - so the whole callback diverges, not just
// its body. Callers write TR5X6_DECOD_EXTI_LCD_Callback() as before.
extern void (*TR5X6_DECOD_EXTI_LCD_Callback)(void);
extern void TR5X6_DECOD_EXTI_BUTT_Callback(void);

/* Variables -----------------------------------------------------------------*/
extern u8 tr5x6_decod_blinks[15];
extern u8 tr5x6_decod_segments[32];
// Sized for the larger machine, both sets always present - see the note at
// the top of tr5x6_decod.c.
// Digits
extern u8 tr5x6_decod_digits[7];
extern u8 tr5x6_decod_digits_flags;
extern const u16 *tr5x6_decod_digits_pos;
// Instruments select
extern u16 tr5x6_decod_inst_sel;
extern u16 tr5x6_decod_inst_sel_flags;
// Accent - driven on the 505 only
extern u8 tr5x6_decod_inst_acc;
extern u8 tr5x6_decod_inst_acc_flags;
// Blocked instruments - driven on the 626 only
extern u16 tr5x6_decod_inst_blk;
extern u16 tr5x6_decod_inst_blk_flags;
// Last step
extern u16 tr5x6_decod_last_step;
extern u16 tr5x6_decod_last_step_flags;
// Step Dots
extern u16 tr5x6_decod_step_dots;
extern u16 tr5x6_decod_step_dots_flags;
// Scale
extern u8 tr5x6_decod_scale;
extern u8 tr5x6_decod_scale_flag;
// Mode
extern u8 tr5x6_decod_mode;
extern u8 tr5x6_decod_mode_flag;
// Group
extern u8 tr5x6_decod_group;
extern u8 tr5x6_decod_group_flag;
// Labels
extern tr5x6_decod_labels_t tr5x6_decod_labels;
extern tr5x6_decod_labels_t tr5x6_decod_labels_flags;

extern tr5x6_decod_buttons_t tr5x6_decod_buttons;
extern tr5x6_decod_buttons_t tr5x6_decod_buttons_flags;

extern u8 segment_test_flag;
extern u8 blink_test_flag;
#endif /* _5X6_DECOD_H_ */
