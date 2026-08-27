/*
 * 5x6 high-flash fixup - declaration
 *
 * See tr5x6_fixup.c for what this does and why it lives in this project.
 */

#ifndef _TR5X6_FIXUP_H
#define _TR5X6_FIXUP_H

// Rewrites the instrument's last flash page into the CURRENT firmware layout.
// Call it once, after the new bootloader image has been written and before the
// tool goes back to answering - interrupts must already be off and the caller
// owns the flash sequence (see BSL_SYSEX_ReleaseHaltState).
//
// Returns 0 on success, negative on a flash failure.
extern s32 TR5X6_FIXUP_HighPage(void);

#endif /* _TR5X6_FIXUP_H */
