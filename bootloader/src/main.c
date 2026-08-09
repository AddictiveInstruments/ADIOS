// $Id: main.c 42 2008-09-30 21:49:43Z tk $
/*
 * MIOS32 Bootloader
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////
//#if defined(MIOS32_FAMILY_STM32F10x)
//# warning "Startup for MIOS32_FAMILY_STM32F10x!"
//#elif defined(MIOS32_FAMILY_STM32F4xx)
//# warning "Startup for MIOS32_FAMILY_STM32F4xx!"
//#elif defined(MIOS32_FAMILY_STM32G0xx)
//# warning "Startup for MIOS32_FAMILY_STM32G0xx!"
//#else
//# warning "Startup Unsupported MIOS32_FAMILY selected!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
//#endif

#include <mios32.h>
#include "bsl_sysex.h"



#if defined(MIOS32_FAMILY_STM32F10x)
#include <usb_lib.h>
#elif defined(MIOS32_FAMILY_STM32F4xx)
#include <usb_core.h>

// imported from mios32_usb.c
extern USB_OTG_CORE_HANDLE  USB_OTG_FS_dev;
#endif


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

// LEDs used by BSL to notify the status
#define BSL_LED_MASK 0x00000001 // green LED

// the "hold" pin - single default (PA11, pull-down/high-active) for every
// currently supported family (F4xx, G0xx) - no per-processor branching here,
// override both BSL_HOLD_PORT/BSL_HOLD_PIN together via the project-facing
// MIOS32_BSL_HOLD_PORT_OVERRIDE/MIOS32_BSL_HOLD_PIN_OVERRIDE below if your
// PCB needs a different pin (relayed by etc/gen_bsl_boundary.sh).
// NOTE: STM32F10x (legacy, out of scope for this project) used a different
// pull-up/low-active wiring - not covered by this default.
#define BSL_HOLD_PORT        GPIOA
#define BSL_HOLD_PIN         LL_GPIO_PIN_11
#define BSL_HOLD_STATE       ((BSL_HOLD_PORT->IDR & BSL_HOLD_PIN) ? 1 : 0)

// optional per-project override, relayed at compile time from the app's own
// mios32_config.h via etc/gen_bsl_boundary.sh into the generated
// mios32_bsl_boundary.h (same channel as MIOS32_APP_FLASH_START_ADDR) - lets
// a project move the physical BSL_HOLD pin to match its own PCB, without
// touching the bootloader source. Falls back to the default above if unset.
#ifdef MIOS32_BSL_HOLD_PORT_OVERRIDE
# undef BSL_HOLD_PORT
# define BSL_HOLD_PORT MIOS32_BSL_HOLD_PORT_OVERRIDE
#endif
#ifdef MIOS32_BSL_HOLD_PIN_OVERRIDE
# undef BSL_HOLD_PIN
# define BSL_HOLD_PIN MIOS32_BSL_HOLD_PIN_OVERRIDE
#endif


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

// note: adaptions also have to be done in MIOS32_BOARD_J5_(Set/Get),
// since these functions access the ports directly
#if defined(MIOS32_FAMILY_STM32F10x) || defined(MIOS32_FAMILY_STM32F4xx)
typedef struct {
  GPIO_TypeDef *port;
  u16 pin_mask;
} srio_pin_t;
#elif defined(MIOS32_FAMILY_LPC17xx)
typedef struct {
  u8 port;
  u8 pin;
} srio_pin_t;
#endif

#if defined(MIOS32_BOARD_MBHP_CORE_STM32)
#define SRIO_RESET_SUPPORTED 0

#define SRIO_GPIO_MODE GPIO_Mode_Out_OD

#define SRIO_NUM_SCLK_PINS 3
static const srio_pin_t srio_sclk_pin[SRIO_NUM_SCLK_PINS] = {
  { GPIOA, LL_GPIO_PIN_5  }, // SPI0
  { GPIOB, LL_GPIO_PIN_13 }, // SPI1
  { GPIOB, LL_GPIO_PIN_6  }, // SPI2
};

#define SRIO_NUM_RCLK_PINS 5
static const srio_pin_t srio_rclk_pin[SRIO_NUM_RCLK_PINS] = {
  { GPIOA, LL_GPIO_PIN_4  }, // SPI0, RCLK1
  { GPIOC, LL_GPIO_PIN_15 }, // SPI0, RCLK2
  { GPIOB, LL_GPIO_PIN_12 }, // SPI1, RCLK1
  { GPIOC, LL_GPIO_PIN_13 }, // SPI2, RCLK1
  { GPIOC, LL_GPIO_PIN_14 }, // SPI2, RCLK2
};

#define SRIO_NUM_MOSI_PINS 3
static const srio_pin_t srio_mosi_pin[SRIO_NUM_MOSI_PINS] = {
  { GPIOA, LL_GPIO_PIN_7  }, // SPI0
  { GPIOB, LL_GPIO_PIN_15 }, // SPI1
  { GPIOB, LL_GPIO_PIN_5  }, // SPI2
};


#elif defined(MIOS32_BOARD_STM32F4DISCOVERY) || defined(MIOS32_BOARD_MBHP_CORE_STM32F4)
#define SRIO_RESET_SUPPORTED 0

#define SRIO_GPIO_MODE  GPIO_Mode_OUT
#define SRIO_GPIO_OTYPE GPIO_OType_PP

#define SRIO_NUM_SCLK_PINS 3
static const srio_pin_t srio_sclk_pin[SRIO_NUM_SCLK_PINS] = {
  { GPIOA, LL_GPIO_PIN_5  }, // SPI0
  { GPIOB, LL_GPIO_PIN_13 }, // SPI1
  { GPIOB, LL_GPIO_PIN_3  }, // SPI2
};

#define SRIO_NUM_RCLK_PINS 6
static const srio_pin_t srio_rclk_pin[SRIO_NUM_RCLK_PINS] = {
  { GPIOB, LL_GPIO_PIN_2  }, // SPI0, RCLK1
  { GPIOD, LL_GPIO_PIN_11 }, // SPI0, RCLK2
  { GPIOB, LL_GPIO_PIN_12 }, // SPI1, RCLK1
  { GPIOD, LL_GPIO_PIN_10 }, // SPI1, RCLK2
  { GPIOA, LL_GPIO_PIN_15 }, // SPI2, RCLK1
  { GPIOB, LL_GPIO_PIN_8  }, // SPI2, RCLK2
};

#define SRIO_NUM_MOSI_PINS 3
static const srio_pin_t srio_mosi_pin[SRIO_NUM_MOSI_PINS] = {
  { GPIOA, LL_GPIO_PIN_7  }, // SPI0
  { GPIOB, LL_GPIO_PIN_15 }, // SPI1
  { GPIOB, LL_GPIO_PIN_5  }, // SPI2
};



#elif defined(MIOS32_BOARD_MBHP_DIPCOREF4)
#define SRIO_RESET_SUPPORTED 0

#define SRIO_GPIO_MODE  LL_GPIO_MODE_OUTPUT
#define SRIO_GPIO_OTYPE LL_GPIO_OUTPUT_PUSHPULL

#define SRIO_NUM_SCLK_PINS 3
static const srio_pin_t srio_sclk_pin[SRIO_NUM_SCLK_PINS] = {
  { GPIOA, LL_GPIO_PIN_5  }, // SPI0
  { GPIOB, LL_GPIO_PIN_13 }, // SPI1
  { GPIOB, LL_GPIO_PIN_3  }, // SPI2
};

#define SRIO_NUM_RCLK_PINS 6
static const srio_pin_t srio_rclk_pin[SRIO_NUM_RCLK_PINS] = {
  { GPIOA, LL_GPIO_PIN_4  }, // SPI0, RCLK1
  { GPIOC, LL_GPIO_PIN_4  }, // SPI0, RCLK2
  { GPIOB, LL_GPIO_PIN_1  }, // SPI1, RCLK1
  { GPIOB, LL_GPIO_PIN_0  }, // SPI1, RCLK2
  { GPIOA, LL_GPIO_PIN_15 }, // SPI2, RCLK1
  { GPIOC, LL_GPIO_PIN_9  }, // SPI2, RCLK2
};

#define SRIO_NUM_MOSI_PINS 3
static const srio_pin_t srio_mosi_pin[SRIO_NUM_MOSI_PINS] = {
  { GPIOA, LL_GPIO_PIN_7  }, // SPI0
  { GPIOC, LL_GPIO_PIN_3  }, // SPI1
  { GPIOB, LL_GPIO_PIN_5  }, // SPI2
};


#elif defined(MIOS32_FAMILY_STM32G0xx)
#define SRIO_RESET_SUPPORTED 0

#else
#define SRIO_RESET_SUPPORTED 0
#endif
//#define BSL_LCD_PRE_RESET
#ifdef BSL_LCD_PRE_RESET
#define LCD_RST_PORT    GPIOB
#define LCD_RST_PIN     LL_GPIO_PIN_0
#endif

/////////////////////////////////////////////////////////////////////////////
// Local prototypes
/////////////////////////////////////////////////////////////////////////////
static s32 ResetSRIOChains(void);

/////////////////////////////////////////////////////////////////////////////
// Main function
/////////////////////////////////////////////////////////////////////////////
int main(void)
{
  ///////////////////////////////////////////////////////////////////////////
  // initialize system
  ///////////////////////////////////////////////////////////////////////////
  MIOS32_SYS_Init(0);
  MIOS32_DELAY_Init(0);

#ifdef BSL_LCD_PRE_RESET
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = LCD_RST_PIN;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
	LL_GPIO_Init(LCD_RST_PORT, &GPIO_InitStruct);
	MIOS32_SYS_STM_PINSET_1(LCD_RST_PORT, LCD_RST_PIN)
	MIOS32_DELAY_Wait_uS(10000);							// wait for 10ms
#endif


  MIOS32_BOARD_LED_Init(BSL_LED_MASK);

  // BSL_HOLD_PIN (PA11)
#if !defined(MIOS32_FAMILY_STM32F10x) && !defined(MIOS32_FAMILY_STM32F4xx)
  {
    LL_GPIO_InitTypeDef GPIO_InitStructure;
    LL_GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStructure.Pin = BSL_HOLD_PIN;
    GPIO_InitStructure.Mode = LL_GPIO_MODE_INPUT;
    GPIO_InitStructure.Pull = LL_GPIO_PULL_DOWN;
    LL_GPIO_Init(BSL_HOLD_PORT, &GPIO_InitStructure);
  }
#endif
  ResetSRIOChains();

  // initialize stopwatch which is used to measure a 2 second delay 
  // before application will be started
  //MIOS32_BOARD_LED_Set(BSL_LED_MASK, 1);
  MIOS32_STOPWATCH_Init(100); // 100 uS accuracy

  //MIOS32_STOPWATCH_Reset();
  //while( MIOS32_STOPWATCH_ValueGet() < 20000){};
  ///////////////////////////////////////////////////////////////////////////
  // get and store state of hold pin
  ///////////////////////////////////////////////////////////////////////////

  // one-shot: consumed here, right at boot, before the upload is known to
  // succeed (the physical BSL_HOLD pin remains the fallback if it doesn't)
  u8 bootloader_mode_requested = MIOS32_SYS_BootloaderModeRequested();
  u8 hold_mode_active_after_reset = BSL_HOLD_STATE | bootloader_mode_requested;
//  if(hold_mode_active_after_reset)fastboot = 0;
  MIOS32_BOARD_LED_Set(BSL_LED_MASK, 1);
  ///////////////////////////////////////////////////////////////////////////
  // initialize USB only if already done (-> not after Power On) or Hold mode enabled
  ///////////////////////////////////////////////////////////////////////////
#ifndef MIOS32_DONT_USE_USB_MIDI
  u8 usb_was_initialized = MIOS32_USB_IsInitialized();
  if( usb_was_initialized || hold_mode_active_after_reset ) {
    // if initialized, this function will only set some variables - it won't re-init the peripheral.
    // if hold mode activated via external pin, force re-initialisation by resetting USB
    if( hold_mode_active_after_reset ) {
#if defined(MIOS32_FAMILY_STM32F10x)
      RCC_APB1PeriphResetCmd(0x00800000, ENABLE); // reset USB device
      RCC_APB1PeriphResetCmd(0x00800000, DISABLE);
#elif defined(MIOS32_FAMILY_STM32F4xx)
      LL_AHB2_GRP1_ForceReset(0x00000080); // reset USB device
      LL_AHB2_GRP1_ReleaseReset(0x00000080);
#endif
    }

    MIOS32_USB_Init(0);
  }
#else
  u8 usb_was_initialized = 0;
#endif

  ///////////////////////////////////////////////////////////////////////////
  // initialize remaining peripherals
  ///////////////////////////////////////////////////////////////////////////
  MIOS32_MIDI_Init(0); // will also initialise UART

  ///////////////////////////////////////////////////////////////////////////
  // initialize SysEx parser
  ///////////////////////////////////////////////////////////////////////////

  BSL_SYSEX_Init(0);

  // relay the software-requested hold (RTC backup flag, consumed above) into
  // the SysEx module: unlike the physical pin it has no manual release, so
  // BSL_SYSEX_ReleaseHaltState() clears it on MIOS Studio's post-upload
  // "reboot" query - otherwise a software-entered BSL could never fall
  // through to the application (found 2026-08-09).
  BSL_SYSEX_SoftHoldSet(bootloader_mode_requested);

#ifdef BSL_UPDATER
  // the BSL-update tool NEVER falls through to an application by itself:
  // its only exits are a successful apply (which resets the core) or a
  // power cycle. Force permanent hold regardless of pin/flag state.
  hold_mode_active_after_reset = 1;
  BSL_SYSEX_SoftHoldSet(1);
#endif


  ///////////////////////////////////////////////////////////////////////////
  // check for optional fast boot
  ///////////////////////////////////////////////////////////////////////////
  u8 fastboot = 0;
#ifdef MIOS32_SYS_ADDR_BSL_INFO_BEGIN
  if( !usb_was_initialized && !hold_mode_active_after_reset ) {
    // read from bootloader info range
    u8 *bsl_info_fastboot_confirm = (u8 *)MIOS32_SYS_ADDR_FASTBOOT_CONFIRM;
    u8 *bsl_info_fastboot = (u8 *)MIOS32_SYS_ADDR_FASTBOOT;
    if( *bsl_info_fastboot_confirm == 0x42 )
      fastboot = *bsl_info_fastboot;
  }
#endif


  ///////////////////////////////////////////////////////////////////////////
  // send upload request to USB and UART MIDI
  ///////////////////////////////////////////////////////////////////////////
  if( !fastboot ) {
#if defined(MIOS32_USE_DIN_MIDI)
    BSL_SYSEX_SendUploadReq(DIN0);
#endif
    if( usb_was_initialized )
      BSL_SYSEX_SendUploadReq(USB0);
  }

#ifdef BSL_LCD_PRE_RESET
	MIOS32_SYS_STM_PINSET_0(LCD_RST_PORT, LCD_RST_PIN);
	MIOS32_DELAY_Wait_uS(15000);							// wait for 10ms
	MIOS32_SYS_STM_PINSET_1(LCD_RST_PORT, LCD_RST_PIN);
#endif
  ///////////////////////////////////////////////////////////////////////////
  // reset stopwatch timer and start wait loop
  if( !fastboot ) {
    MIOS32_STOPWATCH_Reset();
    do {
      // This is a simple way to pulsate a LED via PWM
      // A timer in incrementer mode is used as reference, the counter value is incremented each 100 uS
      // Within the given PWM period, we define a duty cycle based on the current counter value
      // We periodically sweep the PWM duty cycle 100 steps up, and 100 steps down
      u32 cnt = MIOS32_STOPWATCH_ValueGet();  // the reference counter (incremented each 100 uS)
      const u32 pwm_period = 50;       // *100 uS -> 5 mS
      const u32 pwm_sweep_steps = 100; // * 5 mS -> 500 mS
      u32 pwm_duty = ((cnt / pwm_period) % pwm_sweep_steps) / (pwm_sweep_steps/pwm_period);
      if( (cnt % (2*pwm_period*pwm_sweep_steps)) > pwm_period*pwm_sweep_steps )
	pwm_duty = pwm_period-pwm_duty; // negative direction each 50*100 ticks
      u32 led_on = ((cnt % pwm_period) > pwm_duty) ? 1 : 0;
      MIOS32_BOARD_LED_Set(BSL_LED_MASK, led_on ? BSL_LED_MASK : 0);


      // call periodic hook each mS (!!! important - not shorter due to timeout counters which are handled here)
      if( (cnt % 10) == 0 ) {
	MIOS32_MIDI_Periodic_mS();
      }

      // check for incoming MIDI messages - no hooks are used
      // SysEx requests will be parsed by MIOS32 internally, BSL_SYSEX_Cmd() will be called
      // directly by MIOS32 to enhance command set
      MIOS32_MIDI_Receive_Handler(NULL);

    } while( MIOS32_STOPWATCH_ValueGet() < 20000 ||             // wait for 2 seconds
	     BSL_SYSEX_HaltStateGet() ||                        // BSL not halted due to flash write operation
	     (hold_mode_active_after_reset && (BSL_HOLD_STATE || BSL_SYSEX_SoftHoldGet()))); // held by pin, or by the software request (released by Studio's post-upload reboot query)
  }

#if defined(MIOS32_FAMILY_STM32F10x) || defined(MIOS32_FAMILY_STM32F4xx)
  // ensure that flash write access is locked
  LL_FLASH_Lock();
#endif

  // turn off LED
  MIOS32_BOARD_LED_Set(BSL_LED_MASK, 0);

  // branch to application if reset vector is valid (should be inside flash range)
#if defined(MIOS32_FAMILY_STM32F10x) || defined(MIOS32_FAMILY_STM32F4xx) || defined(MIOS32_FAMILY_STM32G0xx)
#if defined (MIOS32_FAMILY_STM32G0xx) || defined(MIOS32_FAMILY_STM32F4xx)
  // application entry: normally the vector table right at the app/bootloader
  // boundary. The one-shot entry override (backup register, set via SysEx
  // command 0x03 - see MIOS32_SYS_AppEntryOverrideGet) redirects a SINGLE
  // boot to an alternate vector table: the one-click BSL update flow uses it
  // to hand control to an updater linked above the normal app origin.
  // Consumed (cleared) here whatever happens: if the alternate target
  // crashes, the next reset falls back to the normal boundary entry instead
  // of looping on a broken override.
  u32 app_vector_base = 0x08000000 + MIOS32_APP_FLASH_START_ADDR;
  {
    u32 entry_override = MIOS32_SYS_AppEntryOverrideGet();
    if( (entry_override >> 24) == 0x08 && !(entry_override & 3) )
      app_vector_base = entry_override;
  }
  u32 *reset_vector = (u32 *)(app_vector_base + 4);
#else
  u32 *reset_vector = (u32 *)0x08004004;
#endif
  if( (*reset_vector >> 24) == 0x08 ) {
    // reset all peripherals
#if defined(MIOS32_FAMILY_STM32F4xx)
	  LL_AHB1_GRP1_ForceReset(0xfffffffe);		// don't reset GPIOA due to USB pins
	  LL_AHB2_GRP1_ForceReset(0xffffff7f);  	// don't reset OTG_FS, so that the connectuion can survive
	  LL_APB1_GRP1_ForceReset(0xffffffff);
	  LL_APB2_GRP1_ForceReset(0xffffffff);
	  LL_AHB1_GRP1_ReleaseReset(0xffffffff);
	  LL_AHB2_GRP1_ReleaseReset(0xffffffff);
	  LL_APB1_GRP1_ReleaseReset(0xffffffff);
	  LL_APB2_GRP1_ReleaseReset(0xffffffff);
#elif defined(MIOS32_FAMILY_STM32G0xx)
	  LL_AHB1_GRP1_ForceReset(LL_AHB1_GRP1_PERIPH_ALL);		// don't reset GPIOA due to USB pins
	  LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_ALL);
	  LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_ALL);
	  LL_AHB1_GRP1_ReleaseReset(LL_AHB1_GRP1_PERIPH_ALL);
	  LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_ALL);
	  LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_ALL);
#endif

    if( hold_mode_active_after_reset ) {
      // if hold mode activated via external pin, force re-initialisation by resetting USB
#if defined(MIOS32_FAMILY_STM32F4xx)
        LL_AHB2_GRP1_ForceReset(0x00000080); // reset USB device
        LL_AHB2_GRP1_ReleaseReset(0x00000080);
#endif
    } else {
      // no hold mode: ensure that USB interrupts won't be triggered while jumping into application
#if defined(MIOS32_FAMILY_STM32F4xx)
#ifndef MIOS32_DONT_USE_USB_MIDI
      if( MIOS32_USB_IsInitialized() && USB_OTG_FS_dev.dev.class_cb != NULL ) {
	USB_OTG_DisableGlobalInt(&USB_OTG_FS_dev);
      }
#endif
#elif defined(MIOS32_FAMILY_STM32G0xx)
      __NOP();
#else
      _SetCNTR(0); // clear USB interrupt mask
      _SetISTR(0); // clear all USB interrupt requests
#endif
    }

#if defined (MIOS32_FAMILY_STM32G0xx) || defined(MIOS32_FAMILY_STM32F4xx)
  // same base as reset_vector above - follows the entry override when set
  u32 *stack_pointer = (u32 *)app_vector_base;
#else
  u32 *stack_pointer = (u32 *)0x08004000;
#endif
    // change stack pointer

#else
# error "please adapt reset sequence for this family"
#endif


    u32 stack_pointer_value = *stack_pointer;
    __asm volatile (		       \
		    "   msr msp, %0\n" \
                    :: "r" (stack_pointer_value) \
                    );

    // branch to application
    void (*application)(void) = (void *)*reset_vector;
    application();
  }

  // otherwise flash LED fast (BSL failed to start application)
  while( 1 ) {
    MIOS32_STOPWATCH_Reset();
    MIOS32_BOARD_LED_Set(BSL_LED_MASK, BSL_LED_MASK);
    while( MIOS32_STOPWATCH_ValueGet() < 2000 );

    MIOS32_STOPWATCH_Reset();
    MIOS32_BOARD_LED_Set(BSL_LED_MASK, 0);
    while( MIOS32_STOPWATCH_ValueGet() < 2000 );
  }

  return 0; // will never be reached
}

/////////////////////////////////////////////////////////////////////////////
// This function resets the SRIO chains at J8/9, J16 and J19 by clocking
// 128 times and strobing the RCLK
//
// It's required since no reset line is available for MBHP_DIN/DOUT modules
//
// It doesn't hurt to shift the clocks to SPI devices as well, since the
// CS line is deactivated during the clocks are sent.
//
// All ports are configured for open drain mode. If push pull is expected, clocks
// won't have an effect as well (no real issue, since SRIOs are always
// used in OD mode)
/////////////////////////////////////////////////////////////////////////////
static s32 ResetSRIOChains(void)
{
#if SRIO_RESET_SUPPORTED == 0
  return -1; // not supported
#else

#if defined(MIOS32_FAMILY_STM32F10x) || defined(MIOS32_FAMILY_STM32F4xx)
  int i;

  LL_GPIO_InitTypeDef GPIO_InitStructure;
  LL_GPIO_StructInit(&GPIO_InitStructure);
  GPIO_InitStructure.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStructure.Mode = SRIO_GPIO_MODE;
#ifdef SRIO_GPIO_OTYPE
  GPIO_InitStructure.OutputType = SRIO_GPIO_OTYPE;
#endif

  // init GPIO driver modes
  for(i=0; i<SRIO_NUM_SCLK_PINS; ++i) {
    MIOS32_SYS_STM_PINSET_1(srio_sclk_pin[i].port, srio_sclk_pin[i].pin_mask); // SCLK=1 by default
    GPIO_InitStructure.Pin = srio_sclk_pin[i].pin_mask;
    LL_GPIO_Init(srio_sclk_pin[i].port, &GPIO_InitStructure);
  }

  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i) {
    MIOS32_SYS_STM_PINSET_1(srio_rclk_pin[i].port, srio_rclk_pin[i].pin_mask); // RCLK=1 by default
    GPIO_InitStructure.Pin = srio_rclk_pin[i].pin_mask;
    LL_GPIO_Init(srio_rclk_pin[i].port, &GPIO_InitStructure);
  }

  for(i=0; i<SRIO_NUM_MOSI_PINS; ++i) {
    MIOS32_SYS_STM_PINSET_0(srio_mosi_pin[i].port, srio_mosi_pin[i].pin_mask); // MOSI=0 by default
    GPIO_InitStructure.Pin = srio_mosi_pin[i].pin_mask;
    LL_GPIO_Init(srio_mosi_pin[i].port, &GPIO_InitStructure);
  }

#if defined(MIOS32_BOARD_STM32F4DISCOVERY) || defined(MIOS32_BOARD_MBHP_CORE_STM32F4)
	// set RE3=1 to ensure that the on-board MEMs is disabled
	GPIO_InitStructure.GPIO_Pin = LL_GPIO_PIN_3;
	GPIO_Init(GPIOE, &GPIO_InitStructure);	
	MIOS32_SYS_STM_PINSET_1(GPIOE, LL_GPIO_PIN_3);
#elif defined(MIOS32_BOARD_MBHP_DIPCOREF4)
 // nothing to do.
#else
# warning "Please doublecheck if RE3 has to be set to 1 to disable MEMs"
#endif

  // send 128 clocks to all SPI ports
  int cycle;
  for(cycle=0; cycle<128; ++cycle) {
    for(i=0; i<SRIO_NUM_SCLK_PINS; ++i) {
      MIOS32_SYS_STM_PINSET_0(srio_sclk_pin[i].port, srio_sclk_pin[i].pin_mask); // SCLK=0
    }

    MIOS32_DELAY_Wait_uS(1);

    for(i=0; i<SRIO_NUM_SCLK_PINS; ++i) {
      MIOS32_SYS_STM_PINSET_1(srio_sclk_pin[i].port, srio_sclk_pin[i].pin_mask); // SCLK=1
    }

    MIOS32_DELAY_Wait_uS(1);
  }

  // latch values
  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i) {
    MIOS32_SYS_STM_PINSET_0(srio_rclk_pin[i].port, srio_rclk_pin[i].pin_mask); // RCLK=0
  }

  MIOS32_DELAY_Wait_uS(1);

  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i) {
    MIOS32_SYS_STM_PINSET_1(srio_rclk_pin[i].port, srio_rclk_pin[i].pin_mask); // RCLK=1
  }

  MIOS32_DELAY_Wait_uS(1);

  // switch back driver modes to input with pull-up enabled
#if defined(MIOS32_FAMILY_STM32F10x)
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
#else
  GPIO_InitStructure.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStructure.Pull = LL_GPIO_PULL_UP;
#endif

  // init GPIO driver modes
  for(i=0; i<SRIO_NUM_SCLK_PINS; ++i) {
    GPIO_InitStructure.Pin = srio_sclk_pin[i].pin_mask;
    LL_GPIO_Init(srio_sclk_pin[i].port, &GPIO_InitStructure);
  }

  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i) {
    GPIO_InitStructure.Pin = srio_rclk_pin[i].pin_mask;
    LL_GPIO_Init(srio_rclk_pin[i].port, &GPIO_InitStructure);
  }

  for(i=0; i<SRIO_NUM_MOSI_PINS; ++i) {
    GPIO_InitStructure.Pin = srio_mosi_pin[i].pin_mask;
    LL_GPIO_Init(srio_mosi_pin[i].port, &GPIO_InitStructure);
  }

  return 0; // no error
#else
  int i;

  // init GPIO driver modes
  for(i=0; i<SRIO_NUM_SCLK_PINS; ++i) {
    MIOS32_SYS_LPC_PINSET(srio_sclk_pin[i].port, srio_sclk_pin[i].pin, 1); // SCLK=1 by default
    MIOS32_SYS_LPC_PINSEL(srio_sclk_pin[i].port, srio_sclk_pin[i].pin, 0); // select GPIO
    MIOS32_SYS_LPC_PINDIR(srio_sclk_pin[i].port, srio_sclk_pin[i].pin, 1); // enable output driver
  }

  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i) {
    MIOS32_SYS_LPC_PINSET(srio_rclk_pin[i].port, srio_rclk_pin[i].pin, 1); // RCLK=1 by default
    MIOS32_SYS_LPC_PINSEL(srio_rclk_pin[i].port, srio_rclk_pin[i].pin, 0); // select GPIO
    MIOS32_SYS_LPC_PINDIR(srio_rclk_pin[i].port, srio_rclk_pin[i].pin, 1); // enable output driver
  }

  for(i=0; i<SRIO_NUM_MOSI_PINS; ++i) {
    MIOS32_SYS_LPC_PINSET(srio_mosi_pin[i].port, srio_mosi_pin[i].pin, 0); // MOSI=0 by default
    MIOS32_SYS_LPC_PINSEL(srio_mosi_pin[i].port, srio_mosi_pin[i].pin, 0); // select GPIO
    MIOS32_SYS_LPC_PINDIR(srio_mosi_pin[i].port, srio_mosi_pin[i].pin, 1); // enable output driver
  }

  // send 128 clocks to all SPI ports
  int cycle;
  for(cycle=0; cycle<128; ++cycle) {
    for(i=0; i<SRIO_NUM_SCLK_PINS; ++i)
      MIOS32_SYS_LPC_PINSET(srio_sclk_pin[i].port, srio_sclk_pin[i].pin, 0); // SCLK=0

    MIOS32_DELAY_Wait_uS(1);

    for(i=0; i<SRIO_NUM_SCLK_PINS; ++i)
      MIOS32_SYS_LPC_PINSET(srio_sclk_pin[i].port, srio_sclk_pin[i].pin, 1); // SCLK=1

    MIOS32_DELAY_Wait_uS(1);
  }

  // latch values
  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i)
    MIOS32_SYS_LPC_PINSET(srio_rclk_pin[i].port, srio_rclk_pin[i].pin, 0); // RCLK=0

  MIOS32_DELAY_Wait_uS(1);

  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i)
    MIOS32_SYS_LPC_PINSET(srio_rclk_pin[i].port, srio_rclk_pin[i].pin, 1); // RCLK=1

  MIOS32_DELAY_Wait_uS(1);

  // switch back driver modes to input with pull-up enabled

  // init GPIO driver modes
  for(i=0; i<SRIO_NUM_SCLK_PINS; ++i)
    MIOS32_SYS_LPC_PINDIR(srio_sclk_pin[i].port, srio_sclk_pin[i].pin, 0); // disable output driver

  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i)
    MIOS32_SYS_LPC_PINDIR(srio_rclk_pin[i].port, srio_rclk_pin[i].pin, 0); // disable output driver

  for(i=0; i<SRIO_NUM_MOSI_PINS; ++i)
    MIOS32_SYS_LPC_PINDIR(srio_mosi_pin[i].port, srio_mosi_pin[i].pin, 0); // disable output driver

  return 0; // no error
#endif

#endif
}
