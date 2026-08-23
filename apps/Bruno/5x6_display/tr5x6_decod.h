/*

 */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _5X6_DECOD_H_
#define _5X6_DECOD_H_

/* Includes ------------------------------------------------------------------*/
#include <adios.h>

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
extern void TR5X6_DECOD_EXTI_LCD_Callback(void);
extern void TR5X6_DECOD_EXTI_BUTT_Callback(void);
#if TR5X6_UNIT_SELECT==626
extern void TR5X6_DECOD_blinks_func(void);
#endif
extern void TR5X6_DECOD_segments_func(void);
#if PATTERN_DETECT
extern s8 TR5X6_DECOD_Group_Get(void);
extern s8 TR5X6_DECOD_Pattern_Get(void);
#endif

/* Variables -----------------------------------------------------------------*/
// How many 7-segment digits this machine has. A VARIABLE, not a #define: the
// day the host is read from the flash magic instead of the build, only the
// assignment in TR5X6_DECOD_Init() changes - nothing that uses it moves.
// It cannot size an array, and that is deliberate.
extern u8 tr5x6_digits_num;
extern u8 tr5x6_decod_blinks[15];
extern u8 tr5x6_decod_segments[32];
#if TR5X6_UNIT_SELECT==505
// Digits
extern u8 tr5x6_decod_digits[6];
extern u8 tr5x6_decod_digits_flags;
extern const u16 tr5x6_decod_digits_pos[6];
// Instruments select
extern u16 tr5x6_decod_inst_sel;
extern u16 tr5x6_decod_inst_sel_flags;
// Accent
extern u8 tr5x6_decod_inst_acc;
extern u8 tr5x6_decod_inst_acc_flags;

#else //TR5X6_UNIT_SELECT==626
extern u8 tr5x6_decod_digits[7];
extern u8 tr5x6_decod_digits_flags;
extern const u16 tr5x6_decod_digits_pos[7];
// Instruments select
extern u16 tr5x6_decod_inst_sel;
extern u16 tr5x6_decod_inst_sel_flags;
// Instruments blinking
extern u16 tr5x6_decod_inst_blk;
extern u16 tr5x6_decod_inst_blk_flags;
#endif
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
