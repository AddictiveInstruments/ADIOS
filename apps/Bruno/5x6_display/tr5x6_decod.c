/*

 */

/* Includes -----------------------------------------------------------------*/
#include "tr5x6_decod.h"
#include "tr5x6_rom.h"
#include <string.h>

/* Defines ------------------------------------------------------------------*/
#define TR5X6_DECOD_CS_PORT 	GPIOC
#define TR5X6_DECOD_CS 	 		LL_GPIO_PIN_13
#define TR5X6_DECOD_CS_IRQn 	EXTI4_15_IRQn
#define TR5X6_DECOD_CLK_PORT 	GPIOC
#define TR5X6_DECOD_CLK 	 	LL_GPIO_PIN_15
#define TR5X6_DECOD_CLK_IRQn 	EXTI4_15_IRQn
#define TR5X6_DECOD_MOSI_PORT 	GPIOC
#define TR5X6_DECOD_MOSI 	 	LL_GPIO_PIN_14

#define TR5X6_DECOD_BUTT_COM_PORT 	GPIOB
#define TR5X6_DECOD_BUTT_COM 	 	LL_GPIO_PIN_3
#define TR5X6_DECOD_BUTT_PORT 		GPIOD
#define TR5X6_DECOD_BUTT_LAST 	 	LL_GPIO_PIN_3
#define TR5X6_DECOD_BUTT_INST 	 	LL_GPIO_PIN_2
#define TR5X6_DECOD_BUTT_DEC 	 	LL_GPIO_PIN_0
#define TR5X6_DECOD_BUTT_INC 	 	LL_GPIO_PIN_1

// The G0 has 2 NVIC priority bits, so only the low two bits of this value
// ever reach the hardware. It used to read 2 on the 505 and 6 on the 626 -
// 6 & 3 = 2, so both landed on the same level and the difference never
// existed outside the source.
#define TR5X6_DECOD_IRQ_PRIOR 	 	2


//#define TR5X6_DECOD_CS_IRQHANDLER_FUNC void EXTI4_15_IRQHandler(void)
//#define TR5X6_DECOD_CLK_IRQHANDLER_FUNC void EXTI2_3_IRQHandler(void)

/* Prototypes -----------------------------------------------------------------*/
static void lcd_callback_505(void);
static void lcd_callback_626(void);
static void segments_func_505(void);
static void segments_func_626(void);
static void blinks_func_626(void);
/* Variables ------------------------------------------------------------------*/
u8 segment=0;
u8 cs_active=0;
u8 decoding=0;
u8 seg_bit=0;


// EVERY host's state lives here, sized for the LARGER machine. Both decoders
// are compiled into both binaries now, so both sets of variables have to
// exist - 133 bytes of RAM on the 505, 3 on the 626, noise against the 36K
// of the G070CB.

// Digits - seven on the 626, six on the 505, so seven here
u8 tr5x6_decod_digits[7];
u8 tr5x6_decod_digits_old[7]={0xff,0xff,0xff,0xff,0xff,0xff,0xff};
u8 tr5x6_decod_digits_flags= 0x00;
// Two const tables in flash, one pointer aimed at boot - the same pattern as
// the slot tables in tr5x6_rom.c.
const u16 tr5x6_decod_digits_pos_505[6]={16,67,107,147,245,285};
const u16 tr5x6_decod_digits_pos_626[7]={16,68,124,157,190,272,305};
const u16 *tr5x6_decod_digits_pos;
// Accent - the 505 only, but the symbol must exist on both
u8 tr5x6_decod_inst_acc;
u8 tr5x6_decod_inst_acc_old= 0x00;
u8 tr5x6_decod_inst_acc_flags= 0x00;
// Blocked instruments - the 626 only, same rule
u16 tr5x6_decod_inst_blk;
u16 tr5x6_decod_inst_blk_old= 0x0000;
u16 tr5x6_decod_inst_blk_flags= 0x0000;
// The captured planes. The 626 fills buff[] first and splits it into the
// other two once the frame checks out; the 505 fills segments[] directly.
u8 tr5x6_decod_segments[32];
u8 tr5x6_decod_segments_shadow[32];
u8 tr5x6_decod_blinks[15];
u8 tr5x6_decod_blinks_shadow[15];
u8 tr5x6_decod_buff[47];
u8 tr5x6_decod_buff_shadow[47];


// Instruments select
u16 tr5x6_decod_inst_sel;
u16 tr5x6_decod_inst_sel_old= 0x0000;
u16 tr5x6_decod_inst_sel_flags= 0x0000;
// Step Dots
u16 tr5x6_decod_step_dots;
u16 tr5x6_decod_step_dots_old= 0x0000;
u16 tr5x6_decod_step_dots_flags= 0x0000;
// Step Dots
u16 tr5x6_decod_last_step;
u16 tr5x6_decod_last_step_old= 0x0000;
u16 tr5x6_decod_last_step_flags= 0x0000;
// Scale
u8 tr5x6_decod_scale;
u8 tr5x6_decod_scale_old=0xff;
u8 tr5x6_decod_scale_flag=0;
// Mode
u8 tr5x6_decod_mode;
u8 tr5x6_decod_mode_old=0x00;
u8 tr5x6_decod_mode_flag;
// Group
u8 tr5x6_decod_group;
u8 tr5x6_decod_group_old=0x00;
u8 tr5x6_decod_group_flag;
// Labels
tr5x6_decod_labels_t tr5x6_decod_labels;
u16 tr5x6_decod_labels_old=0x00;
tr5x6_decod_labels_t tr5x6_decod_labels_flags;

tr5x6_decod_buttons_t tr5x6_decod_buttons;
tr5x6_decod_buttons_t tr5x6_decod_buttons_old;
tr5x6_decod_buttons_t tr5x6_decod_buttons_flags;

//temp flag for testing purpose
u8 blink_test_flag=0;
u8 segment_test_flag=0;

/////////////////////////////////////////////////////////////////////////////
// How the 4 COM bits of ONE SEG address land in a 16-bit field.
//
// This is the PANEL WIRING, and it is identical on both machines - only the
// SEG addresses differ, which is why every one of these tables is shared.
// Note the REVERSED ones: the lower halves of the panel are wired the other
// way round, a fact the TR-626 service notes confirm (see the SEG/COM tables
// in tr5x6_decod.h).
//
// Generated from the original shift-and-mask expressions and verified over
// all 16 nibble values - not transcribed by hand.
/////////////////////////////////////////////////////////////////////////////
enum {
	MAP_STEP_HI_A,  	// nibble -> bits 8,10,12,14   steps/dots 9,11,13,15
	MAP_STEP_HI_B,  	// nibble -> bits 9,11,13,15   steps/dots 10,12,14,16
	MAP_STEP_LO_A,  	// nibble -> bits 6,4,2,0      steps/dots 7,5,3,1  REVERSED
	MAP_STEP_LO_B,  	// nibble -> bits 7,5,3,1      steps/dots 8,6,4,2  REVERSED
	MAP_INST_1_4,   	// nibble -> bits 3,2,1,0      instruments 4,3,2,1 REVERSED
	MAP_INST_5_8,   	// nibble -> bits 4,5,6,7      instruments 5,6,7,8
	MAP_INST_9_12,  	// nibble -> bits 11,10,9,8    instruments 12,11,10,9 REVERSED
	MAP_INST_14_16, 	// nibble -> bits 13,14,15     instruments 14,15,16
	MAP_NUM
};

static const u16 tr5x6_decod_com_map[MAP_NUM][16] = {
	[MAP_STEP_HI_A  ] = { 0x0000, 0x0100, 0x0400, 0x0500, 0x1000, 0x1100, 0x1400, 0x1500, 0x4000, 0x4100, 0x4400, 0x4500, 0x5000, 0x5100, 0x5400, 0x5500 },
	[MAP_STEP_HI_B  ] = { 0x0000, 0x0200, 0x0800, 0x0a00, 0x2000, 0x2200, 0x2800, 0x2a00, 0x8000, 0x8200, 0x8800, 0x8a00, 0xa000, 0xa200, 0xa800, 0xaa00 },
	[MAP_STEP_LO_A  ] = { 0x0000, 0x0040, 0x0010, 0x0050, 0x0004, 0x0044, 0x0014, 0x0054, 0x0001, 0x0041, 0x0011, 0x0051, 0x0005, 0x0045, 0x0015, 0x0055 },
	[MAP_STEP_LO_B  ] = { 0x0000, 0x0080, 0x0020, 0x00a0, 0x0008, 0x0088, 0x0028, 0x00a8, 0x0002, 0x0082, 0x0022, 0x00a2, 0x000a, 0x008a, 0x002a, 0x00aa },
	[MAP_INST_1_4   ] = { 0x0000, 0x0008, 0x0004, 0x000c, 0x0002, 0x000a, 0x0006, 0x000e, 0x0001, 0x0009, 0x0005, 0x000d, 0x0003, 0x000b, 0x0007, 0x000f },
	[MAP_INST_5_8   ] = { 0x0000, 0x0010, 0x0020, 0x0030, 0x0040, 0x0050, 0x0060, 0x0070, 0x0080, 0x0090, 0x00a0, 0x00b0, 0x00c0, 0x00d0, 0x00e0, 0x00f0 },
	[MAP_INST_9_12  ] = { 0x0000, 0x0800, 0x0400, 0x0c00, 0x0200, 0x0a00, 0x0600, 0x0e00, 0x0100, 0x0900, 0x0500, 0x0d00, 0x0300, 0x0b00, 0x0700, 0x0f00 },
	[MAP_INST_14_16 ] = { 0x0000, 0x0000, 0x2000, 0x2000, 0x4000, 0x4000, 0x6000, 0x6000, 0x8000, 0x8000, 0xa000, 0xa000, 0xc000, 0xc000, 0xe000, 0xe000 },
};

/* ********* */
void TR5X6_DECOD_Init()
{
	// FIRST, before anything else in this function.
	// ADIOS_IRQ_Install(EXTI4_15_IRQn) further down arms the interrupt that
	// calls TR5X6_DECOD_EXTI_LCD_Callback, and the host drives its display
	// bus permanently - an edge landing while this pointer is still NULL
	// jumps to address 0 and faults before the screen ever lights up.
	// Assigning here closes that window, and keeps closing it the day the
	// choice comes from tr5x6_unit->magic instead of the build.
	//
	// A ternary, not an #if: both decoders stay referenced, so neither is
	// warned about nor stripped. The two machines do not even raise the
	// same EXTI events, so the whole callback is swapped, not just its body.
	tr5x6_decod_digits_pos        = (TR5X6_UNIT_SELECT==505) ? tr5x6_decod_digits_pos_505
	                                                        : tr5x6_decod_digits_pos_626;
	TR5X6_DECOD_EXTI_LCD_Callback = (TR5X6_UNIT_SELECT==505) ? lcd_callback_505
	                                                        : lcd_callback_626;



	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	LL_EXTI_InitTypeDef EXTI_InitStruct = {0};
	/* GPIO Ports Clock Enable */
	LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);
	LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOC);
	LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOD);

	GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	/**/
	GPIO_InitStruct.Pin = TR5X6_DECOD_BUTT_INST | TR5X6_DECOD_BUTT_LAST | TR5X6_DECOD_BUTT_DEC | TR5X6_DECOD_BUTT_INC;
	LL_GPIO_Init(TR5X6_DECOD_BUTT_PORT, &GPIO_InitStruct);
	/**/
	GPIO_InitStruct.Pin = TR5X6_DECOD_BUTT_COM;
	LL_GPIO_Init(TR5X6_DECOD_BUTT_COM_PORT, &GPIO_InitStruct);
	// EXTI TR5X6_DECOD_BUTT_COM
	LL_EXTI_SetEXTISource(LL_EXTI_CONFIG_PORTB, LL_EXTI_CONFIG_LINE3);
	EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_3;
	EXTI_InitStruct.LineCommand = ENABLE;
	EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
	EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING;
	LL_EXTI_Init(&EXTI_InitStruct);

	GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	/**/
	GPIO_InitStruct.Pin = TR5X6_DECOD_CS;
	LL_GPIO_Init(TR5X6_DECOD_CS_PORT, &GPIO_InitStruct);
	/**/
	GPIO_InitStruct.Pin = TR5X6_DECOD_CLK;
	LL_GPIO_Init(TR5X6_DECOD_CLK_PORT, &GPIO_InitStruct);
	/**/
	GPIO_InitStruct.Pin = TR5X6_DECOD_MOSI;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	LL_GPIO_Init(TR5X6_DECOD_MOSI_PORT, &GPIO_InitStruct);
	// EXTI TR5X6_DECOD_CS
	LL_EXTI_SetEXTISource(LL_EXTI_CONFIG_PORTC, LL_EXTI_CONFIG_LINE13);
	EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_13;
	EXTI_InitStruct.LineCommand = ENABLE;
	EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
	// The 626 validates its frame on the CS rising edge; the 505 has no use
	// for that edge at all - see the bus map in tr5x6_decod.h.
	EXTI_InitStruct.Trigger = tr5x6_unit->cs_two_edges ? LL_EXTI_TRIGGER_RISING_FALLING
	                                                   : LL_EXTI_TRIGGER_FALLING;
	LL_EXTI_Init(&EXTI_InitStruct);
	// EXTI TR5X6_DECOD_CLK
	LL_EXTI_SetEXTISource(LL_EXTI_CONFIG_PORTC, LL_EXTI_CONFIG_LINE15);
	EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_15;
	EXTI_InitStruct.LineCommand = ENABLE;
	EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
	EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING;
	LL_EXTI_Init(&EXTI_InitStruct);

	/* EXTI interrupt init*/
	ADIOS_IRQ_Install(EXTI4_15_IRQn, TR5X6_DECOD_IRQ_PRIOR);
	ADIOS_IRQ_Install(EXTI2_3_IRQn, TR5X6_DECOD_IRQ_PRIOR);


	//temp
	  LL_GPIO_StructInit(&GPIO_InitStruct);
	  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
	  GPIO_InitStruct.Pin = LL_GPIO_PIN_7;
	  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
	  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;

	  // init IO mode
	  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	  ADIOS_SYS_STM_PINSET(GPIOC, LL_GPIO_PIN_7, 0);

	tr5x6_decod_segments[0]=0;
	decoding =0;
	cs_active=0;
	segment=0;
	seg_bit=0;
	tr5x6_decod_buff[0]=0;	// only the 626 fills it, but it exists on both
	tr5x6_decod_buttons_old.ALL = 0xf;
	tr5x6_decod_buttons.ALL = 0xf;

}



/* ********* */
void TR5X6_DECOD_EXTI_BUTT_Callback(void)
{
	if (LL_EXTI_IsActiveRisingFlag_0_31(LL_EXTI_LINE_3) != RESET)
		//if(EXTI->FPR1 & EXTI_FPR1_FPIF3)
	{
		//ADIOS_BOARD_LED_Set(1, 1);
		LL_EXTI_ClearRisingFlag_0_31(LL_EXTI_LINE_3);
		//ADIOS_DELAY_Wait_uS(5);
		tr5x6_decod_buttons.ALL=(u8)(TR5X6_DECOD_BUTT_PORT->IDR & 0xf);
		//if(tr5x6_decod_buttons.ALL)
		//		tr5x6_decod_buttons.last = ADIOS_SYS_STM_PINGET(TR5X6_DECOD_BUTT_PORT, TR5X6_DECOD_BUTT_LAST)?0:1;
		//		//tr5x6_decod_buttons.inst = ADIOS_SYS_STM_PINGET(TR5X6_DECOD_BUTT_PORT, TR5X6_DECOD_BUTT_INST)?0:1;
		//		tr5x6_decod_buttons.inc = ADIOS_SYS_STM_PINGET(TR5X6_DECOD_BUTT_PORT, TR5X6_DECOD_BUTT_INC)?0:1;
		//		tr5x6_decod_buttons.dec = ADIOS_SYS_STM_PINGET(TR5X6_DECOD_BUTT_PORT, TR5X6_DECOD_BUTT_DEC)?0:1;
		//		//s32 led =  ADIOS_BOARD_LED_Get();


		//EXTI->FPR1 |=EXTI_FPR1_FPIF3;
		//ADIOS_BOARD_LED_Set(1, 0);
	}

}

/* ********* */
tr5x6_decod_buttons_t TR5X6_DECOD_BUTT_Handler(void)
{
	if(tr5x6_decod_buttons_old.ALL != tr5x6_decod_buttons.ALL){
		tr5x6_decod_buttons_flags.ALL = tr5x6_decod_buttons.ALL ^ tr5x6_decod_buttons_old.ALL;
		tr5x6_decod_buttons_old.ALL = tr5x6_decod_buttons.ALL;
	}
	return tr5x6_decod_buttons_flags;
}


/* ********* */
void (*TR5X6_DECOD_EXTI_LCD_Callback)(void);

/////////////////////////////////////////////////////////////////////////////
// TR-505 : two events. CS falling opens a frame, each CLK rising edge
// captures one bit straight into tr5x6_decod_segments[]. The 32nd byte ends
// the frame and triggers the decode.
/////////////////////////////////////////////////////////////////////////////
static void lcd_callback_505(void)
{
	// CLK/MISO lines
	if (LL_EXTI_IsActiveRisingFlag_0_31(LL_EXTI_LINE_15) != RESET)
	{
		LL_EXTI_ClearRisingFlag_0_31(LL_EXTI_LINE_15);
		if(segment>31 || !cs_active)return;
		//ADIOS_IRQ_Disable();
		if(ADIOS_SYS_STM_PINGET(TR5X6_DECOD_MOSI_PORT, TR5X6_DECOD_MOSI))
			tr5x6_decod_segments[segment] |= (0x80>>seg_bit);
		else
			tr5x6_decod_segments[segment] &= ~(0x80>>seg_bit);
		seg_bit++;
		if(seg_bit>=8){
			seg_bit = 0;
			segment++;
			if(segment>31){
				cs_active=0;

				segments_func_505();
			}
		}
	}
	// CS line
	if (LL_EXTI_IsActiveFallingFlag_0_31(LL_EXTI_LINE_13) != RESET)
	{
		LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_13);
		if(segment>31)return;
		cs_active=1;
		segment=0;
		seg_bit=0;
	}
}

/////////////////////////////////////////////////////////////////////////////
// TR-626 : three events. Bits land in tr5x6_decod_buff[], and the frame is
// validated on the CS RISING edge - an event the 505 does not have at all.
// The frame is 47 bytes: 15 of blink data then 32 of segments, split into
// their own arrays only once the whole thing checks out.
/////////////////////////////////////////////////////////////////////////////
static void lcd_callback_626(void)
{
	// CLK/MISO lines
	if (LL_EXTI_IsActiveRisingFlag_0_31(LL_EXTI_LINE_15) != RESET)
	{
		LL_EXTI_ClearRisingFlag_0_31(LL_EXTI_LINE_15);
		if(decoding)return;
		if(!cs_active || (segment>=47) )return;
		//ADIOS_IRQ_Disable();
		//if(decod_reg!=NULL){
		if(ADIOS_SYS_STM_PINGET(TR5X6_DECOD_MOSI_PORT, TR5X6_DECOD_MOSI))
			tr5x6_decod_buff[segment] |= (0x80>>seg_bit);
		else
			tr5x6_decod_buff[segment] &= ~(0x80>>seg_bit);
		//}
		seg_bit++;
		if(seg_bit>=8){
			seg_bit = 0;
			if(tr5x6_decod_buff[0]==0xf2){
				segment++;
			}else{
				cs_active=0;
			}
		}
	}
	// CS line
	if (LL_EXTI_IsActiveFallingFlag_0_31(LL_EXTI_LINE_13) != RESET)
	{
		LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_13);
		if(decoding)return;
		if(!cs_active || ((cs_active==1) && (segment!=15)) ){
			segment=0;
			seg_bit=0;
			tr5x6_decod_buff[0]=0;

		}
		cs_active++;
	}
	if (LL_EXTI_IsActiveRisingFlag_0_31(LL_EXTI_LINE_13) != RESET)
	{
		LL_EXTI_ClearRisingFlag_0_31(LL_EXTI_LINE_13);
		if(decoding)return;
		if( (cs_active>2) || (segment> 47)){
			cs_active=0;
			segment=0;
			seg_bit=0;
		}
		if( (cs_active==2) && (segment==47) && (seg_bit==0) ){
			// here we check the data
			decoding = 0;
			cs_active=0;
			segment=0;
			seg_bit=0;
			for(int i=0;i<47;i++){
				if(tr5x6_decod_buff[i]!=tr5x6_decod_buff_shadow[i]){
					decoding = 1;
					break;
				}
			}


			for(int i=1;i<15;i++)if((tr5x6_decod_buff[i]&0xf0)!=0xc0){decoding =0; break;}
			for(int i=15;i<47;i++)if((tr5x6_decod_buff[i]&0xf0)!=0xd0){decoding =0; break;}

			// if valid and changes, decoding request;
			if(decoding==1){
				ADIOS_SYS_STM_PINSET(GPIOC, LL_GPIO_PIN_7, 1);
				memcpy(&tr5x6_decod_blinks[0], &tr5x6_decod_buff[0], sizeof(u8)*15);
				memcpy(&tr5x6_decod_segments[0], &tr5x6_decod_buff[15], sizeof(u8)*32);
				memcpy(&tr5x6_decod_buff_shadow[0], &tr5x6_decod_buff[0], sizeof(u8)*47);
				blinks_func_626();
				segments_func_626();
				ADIOS_SYS_STM_PINSET(GPIOC, LL_GPIO_PIN_7, 0);
				//decoding = 0;
			}

		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// TR-626 only: the 15-byte blink frame carries which instruments are blocked
/////////////////////////////////////////////////////////////////////////////
static void blinks_func_626(void)
{
	// Instruments select
	tr5x6_decod_inst_blk= tr5x6_decod_com_map[MAP_INST_14_16 ][tr5x6_decod_blinks[1]&0xf]
	                     | tr5x6_decod_com_map[MAP_INST_5_8   ][tr5x6_decod_blinks[2]&0xf]
	                     | tr5x6_decod_com_map[MAP_INST_1_4   ][tr5x6_decod_blinks[12]&0xf]
	                     | tr5x6_decod_com_map[MAP_INST_9_12  ][tr5x6_decod_blinks[13]&0xf]
	                     | ((tr5x6_decod_blinks[14]&0x1)<<12);	// instrument 13
	tr5x6_decod_inst_blk_flags |= tr5x6_decod_inst_blk ^ tr5x6_decod_inst_blk_old;
	tr5x6_decod_inst_blk_old = tr5x6_decod_inst_blk;

	if(memcmp(tr5x6_decod_blinks, tr5x6_decod_blinks_shadow, 15))blink_test_flag=1;
	memcpy(tr5x6_decod_blinks_shadow, tr5x6_decod_blinks, 15);
}

/////////////////////////////////////////////////////////////////////////////
// TR-505 : turns a 32-byte segment frame into the decoded state
/////////////////////////////////////////////////////////////////////////////
static void segments_func_505(void)
{
	if(cs_active)return;

	// Every byte of the segment plane is a uPD7225 WRITE DATA MEMORY
	// instruction, 1101 dddd - see the bus map in tr5x6_decod.h. A frame
	// whose capture started mid-transfer is SHIFTED, and its bytes no
	// longer carry that opcode: decoding it puts garbage on screen for a
	// moment. The 626 has validated its frames since day one; the 505
	// never did, and that stayed invisible only because this function
	// used to be slow enough to drop such frames on its own. Now that it
	// is fast, they get through - hence this check.
	for(int i=0;i<32;i++)
		if((tr5x6_decod_segments[i]&0xf0)!=0xd0){ segment=0; return; }

	// All digits
	tr5x6_decod_digits[0]= ((tr5x6_decod_segments[0]&0xf)<<3) | ((tr5x6_decod_segments[1]&0xe)>>1);
	tr5x6_decod_digits[1]= ((tr5x6_decod_segments[2]&0xf)<<3) | ((tr5x6_decod_segments[3]&0xe)>>1);
	tr5x6_decod_digits[2]= ((tr5x6_decod_segments[4]&0xf)<<3) | ((tr5x6_decod_segments[5]&0xe)>>1);
	tr5x6_decod_digits[3]= ((tr5x6_decod_segments[6]&0xf)<<3) | ((tr5x6_decod_segments[7]&0xe)>>1);
	tr5x6_decod_digits[4]= ((tr5x6_decod_segments[10]&0xf)<<3) | ((tr5x6_decod_segments[11]&0xe)>>1);
	tr5x6_decod_digits[5]= ((tr5x6_decod_segments[12]&0xf)<<3) | ((tr5x6_decod_segments[13]&0xe)>>1);
	for(int i=0;i<6;i++){
		if(tr5x6_decod_digits[i]!=tr5x6_decod_digits_old[i]){
			tr5x6_decod_digits_old[i]=tr5x6_decod_digits[i];
			tr5x6_decod_digits_flags |= (1<<i);
		}
	}


	// Instruments select
	tr5x6_decod_inst_acc = (tr5x6_decod_segments[28]>>2)&3;
	tr5x6_decod_inst_acc_flags |= tr5x6_decod_inst_acc ^ tr5x6_decod_inst_acc_old;
	tr5x6_decod_inst_acc_old    = tr5x6_decod_inst_acc;
	tr5x6_decod_inst_sel= tr5x6_decod_com_map[MAP_INST_14_16 ][tr5x6_decod_segments[15]&0xf]
	                     | tr5x6_decod_com_map[MAP_INST_5_8   ][tr5x6_decod_segments[16]&0xf]
	                     | tr5x6_decod_com_map[MAP_INST_1_4   ][tr5x6_decod_segments[26]&0xf]
	                     | tr5x6_decod_com_map[MAP_INST_9_12  ][tr5x6_decod_segments[27]&0xf]
	                     | ((tr5x6_decod_segments[28]&0x1)<<12);	// instrument 13
	tr5x6_decod_inst_sel_flags |= tr5x6_decod_inst_sel ^ tr5x6_decod_inst_sel_old;
	tr5x6_decod_inst_sel_old = tr5x6_decod_inst_sel;
	// Step Dots
	tr5x6_decod_step_dots= tr5x6_decod_com_map[MAP_STEP_HI_A  ][tr5x6_decod_segments[19]&0xf]
	                     | tr5x6_decod_com_map[MAP_STEP_HI_B  ][tr5x6_decod_segments[20]&0xf]
	                     | tr5x6_decod_com_map[MAP_STEP_LO_A  ][tr5x6_decod_segments[22]&0xf]
	                     | tr5x6_decod_com_map[MAP_STEP_LO_B  ][tr5x6_decod_segments[23]&0xf];
	tr5x6_decod_step_dots_flags |= tr5x6_decod_step_dots ^ tr5x6_decod_step_dots_old;
	tr5x6_decod_step_dots_old = tr5x6_decod_step_dots;

	// Last step
	tr5x6_decod_last_step= tr5x6_decod_com_map[MAP_STEP_HI_A  ][tr5x6_decod_segments[17]&0xf]
	                     | tr5x6_decod_com_map[MAP_STEP_HI_B  ][tr5x6_decod_segments[18]&0xf]
	                     | tr5x6_decod_com_map[MAP_STEP_LO_A  ][tr5x6_decod_segments[24]&0xf]
	                     | tr5x6_decod_com_map[MAP_STEP_LO_B  ][tr5x6_decod_segments[25]&0xf];
	tr5x6_decod_last_step_flags |= tr5x6_decod_last_step ^ tr5x6_decod_last_step_old;
	tr5x6_decod_last_step_old = tr5x6_decod_last_step;
	// Scale
	tr5x6_decod_scale=tr5x6_decod_segments[21] & 0xf;
	if(tr5x6_decod_scale_old != tr5x6_decod_scale){
		tr5x6_decod_scale_flag=1;
	}
	tr5x6_decod_scale_old = tr5x6_decod_scale;
	// Mode
	tr5x6_decod_mode=((tr5x6_decod_segments[13]&0x1) | ((tr5x6_decod_segments[14]&0xf)<<1));
	if(tr5x6_decod_mode_old != tr5x6_decod_mode){
		tr5x6_decod_mode_flag=1;
	}
	tr5x6_decod_mode_old = tr5x6_decod_mode;
	// Group
	tr5x6_decod_group=(((tr5x6_decod_segments[8]&0xe)>>1) | ((tr5x6_decod_segments[9]&0xe)<<2));
	if(tr5x6_decod_group_old != tr5x6_decod_group){
		tr5x6_decod_group_flag=1;
	}
	tr5x6_decod_group_old = tr5x6_decod_group;
	// Labels
	tr5x6_decod_labels.tempo = tr5x6_decod_segments[1]&0x1;
	tr5x6_decod_labels.measure = tr5x6_decod_segments[3]&0x1;
	tr5x6_decod_labels.sync = tr5x6_decod_segments[5]&0x1;
	tr5x6_decod_labels.grp_pat = tr5x6_decod_segments[7]&0x1;
	tr5x6_decod_labels.chain = tr5x6_decod_segments[8]&0x1;
	tr5x6_decod_labels.level = tr5x6_decod_segments[9]&0x1;
	tr5x6_decod_labels.block = tr5x6_decod_segments[11]&0x1;
	tr5x6_decod_labels_flags.ALL |= tr5x6_decod_labels.ALL ^ tr5x6_decod_labels_old;
	tr5x6_decod_labels_old = tr5x6_decod_labels.ALL;
	segment=0;
}

/////////////////////////////////////////////////////////////////////////////
// TR-626 : same job, different bus layout - and one more digit
/////////////////////////////////////////////////////////////////////////////
static void segments_func_626(void)
{
	// All digits
	tr5x6_decod_digits[0]= ((tr5x6_decod_segments[0]&0xf)<<3) | ((tr5x6_decod_segments[1]&0xe)>>1);
	tr5x6_decod_digits[1]= ((tr5x6_decod_segments[2]&0xf)<<3) | ((tr5x6_decod_segments[3]&0xe)>>1);
	tr5x6_decod_digits[2]= ((tr5x6_decod_segments[4]&0xf)<<3) | ((tr5x6_decod_segments[5]&0xe)>>1);
	tr5x6_decod_digits[3]= ((tr5x6_decod_segments[6]&0xf)<<3) | ((tr5x6_decod_segments[7]&0xe)>>1);
	tr5x6_decod_digits[4]= ((tr5x6_decod_segments[9]&0xf)<<3) | ((tr5x6_decod_segments[10]&0xe)>>1);
	tr5x6_decod_digits[5]= ((tr5x6_decod_segments[13]&0xf)<<3) | ((tr5x6_decod_segments[14]&0xe)>>1);
	tr5x6_decod_digits[6]= ((tr5x6_decod_segments[15]&0xf)<<3) | ((tr5x6_decod_segments[16]&0xe)>>1);
	for(int i=0;i<7;i++){
		if(tr5x6_decod_digits[i]!=tr5x6_decod_digits_old[i]){
			tr5x6_decod_digits_old[i]=tr5x6_decod_digits[i];
			tr5x6_decod_digits_flags |= (1<<i);
		}
	}
	// Instruments select
	tr5x6_decod_inst_sel= tr5x6_decod_com_map[MAP_INST_14_16 ][tr5x6_decod_segments[18]&0xf]
	                     | tr5x6_decod_com_map[MAP_INST_5_8   ][tr5x6_decod_segments[19]&0xf]
	                     | tr5x6_decod_com_map[MAP_INST_1_4   ][tr5x6_decod_segments[29]&0xf]
	                     | tr5x6_decod_com_map[MAP_INST_9_12  ][tr5x6_decod_segments[30]&0xf]
	                     | ((tr5x6_decod_segments[31]&0x1)<<12);	// instrument 13
	tr5x6_decod_inst_sel_flags |= tr5x6_decod_inst_sel ^ tr5x6_decod_inst_sel_old;
	tr5x6_decod_inst_sel_old = tr5x6_decod_inst_sel;

	// Step Dots
	tr5x6_decod_step_dots= tr5x6_decod_com_map[MAP_STEP_HI_A  ][tr5x6_decod_segments[22]&0xf]
	                     | tr5x6_decod_com_map[MAP_STEP_HI_B  ][tr5x6_decod_segments[23]&0xf]
	                     | tr5x6_decod_com_map[MAP_STEP_LO_A  ][tr5x6_decod_segments[25]&0xf]
	                     | tr5x6_decod_com_map[MAP_STEP_LO_B  ][tr5x6_decod_segments[26]&0xf];
	tr5x6_decod_step_dots_flags |= tr5x6_decod_step_dots ^ tr5x6_decod_step_dots_old;
	tr5x6_decod_step_dots_old = tr5x6_decod_step_dots;

	// Last step
	tr5x6_decod_last_step= tr5x6_decod_com_map[MAP_STEP_HI_A  ][tr5x6_decod_segments[20]&0xf]
	                     | tr5x6_decod_com_map[MAP_STEP_HI_B  ][tr5x6_decod_segments[21]&0xf]
	                     | tr5x6_decod_com_map[MAP_STEP_LO_A  ][tr5x6_decod_segments[27]&0xf]
	                     | tr5x6_decod_com_map[MAP_STEP_LO_B  ][tr5x6_decod_segments[28]&0xf];
	tr5x6_decod_last_step_flags |= tr5x6_decod_last_step ^ tr5x6_decod_last_step_old;
	tr5x6_decod_last_step_old = tr5x6_decod_last_step;

	// Scale
	tr5x6_decod_scale=tr5x6_decod_segments[24] & 0xf;
	if(tr5x6_decod_scale_old != tr5x6_decod_scale){
		tr5x6_decod_scale_flag=1;
	}
	tr5x6_decod_scale_old = tr5x6_decod_scale;
	// Mode
	tr5x6_decod_mode=((tr5x6_decod_segments[18]&0x1) | ((tr5x6_decod_segments[17]&0x7)<<2) | ((tr5x6_decod_segments[16]&0x1)<<1));
	if(tr5x6_decod_mode_old != tr5x6_decod_mode){
		tr5x6_decod_mode_flag=1;
	}
	tr5x6_decod_mode_old = tr5x6_decod_mode;
	// Group
	tr5x6_decod_group=(((tr5x6_decod_segments[11]&0xe)>>1) | ((tr5x6_decod_segments[12]&0xe)<<2));
	if(tr5x6_decod_group_old != tr5x6_decod_group){
		tr5x6_decod_group_flag=1;
	}
	tr5x6_decod_group_old = tr5x6_decod_group;

	// Labels
	tr5x6_decod_labels.card = tr5x6_decod_segments[1]&0x1;
	tr5x6_decod_labels.tempo = tr5x6_decod_segments[3]&0x1;
	tr5x6_decod_labels.measure = tr5x6_decod_segments[5]&0x1;
	tr5x6_decod_labels.sync = (tr5x6_decod_segments[17]&0x8)?1:0;
	tr5x6_decod_labels.pitch = tr5x6_decod_segments[7]&0x1;
	tr5x6_decod_labels.level = tr5x6_decod_segments[10]&0x1;
	tr5x6_decod_labels.shuffle = (tr5x6_decod_segments[8]&0x8)?1:0;
	tr5x6_decod_labels.flam = tr5x6_decod_segments[8]&0x1;
	tr5x6_decod_labels.accent = (tr5x6_decod_segments[31]&0x4)?1:0;
	tr5x6_decod_labels.chain = tr5x6_decod_segments[11]&0x1;
	tr5x6_decod_labels.block = tr5x6_decod_segments[14]&0x1;
	tr5x6_decod_labels_flags.ALL |= tr5x6_decod_labels.ALL ^ tr5x6_decod_labels_old;
	tr5x6_decod_labels_old = tr5x6_decod_labels.ALL;


	if(memcmp(tr5x6_decod_segments, tr5x6_decod_segments_shadow, 32))segment_test_flag=1;
	memcpy(tr5x6_decod_segments_shadow, tr5x6_decod_segments, 32);
	//ADIOS_IRQ_Install(EXTI4_15_IRQn, TR5X6_DECOD_IRQ_PRIOR);
	//decoding &=0xfe;
	decoding=0;
}

