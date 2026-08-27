/*
 * ADIOS Bootloader
 *
 * ==========================================================================
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
//#if defined(ADIOS_FAMILY_STM32F10x)
//# warning "Startup for ADIOS_FAMILY_STM32F10x!"
//#elif defined(ADIOS_FAMILY_STM32F4xx)
//# warning "Startup for ADIOS_FAMILY_STM32F4xx!"
//#elif defined(ADIOS_FAMILY_STM32G0xx)
//# warning "Startup for ADIOS_FAMILY_STM32G0xx!"
//#else
//# warning "Startup Unsupported ADIOS_FAMILY selected!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
//#endif

#include <adios.h>
#include "bsl_sysex.h"


// Nothing to include for USB: this file no longer reaches into the
// controller. The hand-over goes through ADIOS_USB_HandoffPrepare(), which
// leaves the session ALIVE for the application to adopt.


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

// the "hold" pin - single default (PA11, pull-down/high-active) for every
// currently supported family (F4xx, G0xx) - no per-processor branching here,
// override both BSL_HOLD_PORT/BSL_HOLD_PIN together via the project-facing
// ADIOS_BSL_HOLD_PORT_OVERRIDE/ADIOS_BSL_HOLD_PIN_OVERRIDE below if your
// PCB needs a different pin (relayed by etc/gen_bsl_boundary.sh).
// NOTE: STM32F10x (legacy, out of scope for this project) used a different
// pull-up/low-active wiring - not covered by this default.
#define BSL_HOLD_PORT        GPIOA
#define BSL_HOLD_PIN         LL_GPIO_PIN_11
#define BSL_HOLD_STATE       ((BSL_HOLD_PORT->IDR & BSL_HOLD_PIN) ? 1 : 0)

// optional per-project override, relayed at compile time from the app's own
// adios_config.h via etc/gen_bsl_boundary.sh into the generated
// adios_bsl_boundary.h (same channel as ADIOS_APP_FLASH_START_ADDR) - lets
// a project move the physical BSL_HOLD pin to match its own PCB, without
// touching the bootloader source. Falls back to the default above if unset.
#ifdef ADIOS_BSL_HOLD_PORT_OVERRIDE
# undef BSL_HOLD_PORT
# define BSL_HOLD_PORT ADIOS_BSL_HOLD_PORT_OVERRIDE
#endif
#ifdef ADIOS_BSL_HOLD_PIN_OVERRIDE
# undef BSL_HOLD_PIN
# define BSL_HOLD_PIN ADIOS_BSL_HOLD_PIN_OVERRIDE
#endif


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

#if defined(ADIOS_FAMILY_STM32F10x) || defined(ADIOS_FAMILY_STM32F4xx)
typedef struct {
  GPIO_TypeDef *port;
  u16 pin_mask;
} srio_pin_t;
#elif defined(ADIOS_FAMILY_LPC17xx)
typedef struct {
  u8 port;
  u8 pin;
} srio_pin_t;
#endif

// The shift-register pins the bootloader strobes before handing over. This
// is board wiring, not silicon: a project that wires them elsewhere says so
// through its BSL_RELAY block (see the projects that carry one).
#if defined(ADIOS_FAMILY_STM32F4xx)
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


#elif defined(ADIOS_FAMILY_STM32G0xx)
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
static void BSL_WaitLoop(u8 hold_mode_active_after_reset, u8 usb_was_initialized, u8 no_app);

/////////////////////////////////////////////////////////////////////////////
// The bootloader's MIDI service loop: announces that it is ready to receive
// an upload, pulsates the LED and dispatches incoming SysEx (parsed by ADIOS
// internally, which calls back into BSL_SYSEX_Cmd).
//
// main() enters it from exactly two places, and they are the only two reasons
// to be here at all:
//   - held, by the BSL_HOLD pin or by the software request an application
//     sets before rebooting on ADIOS Studio's behalf (no_app = 0). Leaves when
//     released, and main() hands over to the application below.
//   - no valid application to hand over to (no_app = 1), the state a power cut
//     during an upload leaves behind. Leaves as soon as something HAS been
//     uploaded and released, upon which main() resets and the whole boot
//     decision is taken again - which is what lets a device recover without a
//     power cycle, and what lets the entry override be honoured.
//
// The LED tells the two apart: pulsating = held, fast blink = no application.
//
// There is deliberately NO timeout. The former 2-second window existed so
// ADIOS Studio could catch a core it had just asked to reboot; the software
// hold flag does that explicitly now, so the delay only ever slowed down
// every single boot (it was disabled outright by the "fastboot" info-block
// field, which this makes pointless - see main()).
/////////////////////////////////////////////////////////////////////////////
static void BSL_WaitLoop(u8 hold_mode_active_after_reset, u8 usb_was_initialized, u8 no_app)
{
  // tell whoever is listening that this core takes uploads now. Belongs HERE
  // rather than in main(): every path that waits must announce, and the one
  // that hands over immediately has nothing to announce.
#if defined(ADIOS_USE_DIN_MIDI)
  // ADIOS_MIDI_DEFAULT_PORT, not a hardcoded DIN0: this bootloader talks
  // on whatever DIN port its board actually wires (see adios_config.h).
  // Announcing on a port that isn't even enabled makes the core look dead
  // to ADIOS Studio while everything else works - which is exactly what
  // happened once ADIOS_UARTn was realigned to USART(n+1) and this
  // board's MIDI connector became DIN2 instead of DIN0.
  BSL_SYSEX_SendUploadReq(ADIOS_MIDI_DEFAULT_PORT);
#endif
  // On USB the announcement cannot be made here: the port does not exist
  // yet. Coming up means letting go of the bus first, so that the host sees
  // a NEW device rather than the one it already knew, and enumeration only
  // finishes some tens of milliseconds later - announcing now would be
  // announcing to nobody. The loop below does it once the port is there.
  //
  // It used to be sent from here, guarded by "was USB already up?". That
  // made sense when the connection survived a reset and the host was still
  // talking to us. It cannot be answered from RAM any more: the flag it read
  // is cleared at start-up, so the guard was always false and the
  // announcement never went out at all - which is precisely what a host
  // waits for before it starts an upload.
#if defined(ADIOS_USE_USB_MIDI)
  u32 usb_announced_at = 0;
  u8  usb_announced_once = 0;
#endif
  (void)usb_was_initialized;

  ADIOS_STOPWATCH_Reset();
  do {
    // This is a simple way to pulsate a LED via PWM
    // A timer in incrementer mode is used as reference, the counter value is incremented each 100 uS
    // Within the given PWM period, we define a duty cycle based on the current counter value
    // We periodically sweep the PWM duty cycle 100 steps up, and 100 steps down
    u32 cnt = ADIOS_STOPWATCH_ValueGet();  // the reference counter (incremented each 100 uS)
    // the stopwatch is a 16-bit timer at 100 uS: it overruns after 6.55 s and
    // ValueGet() then returns 0xffffffff for good. Every LED pattern below is
    // derived from this counter, so without restarting it the LED freezes -
    // lit, in the fast-blink case - a few seconds after the last SysEx (which
    // is what BSL_SYSEX_Cmd's own reset used to hide during an upload)
    if( cnt == 0xffffffff ) {
      ADIOS_STOPWATCH_Reset();
      cnt = 0;
    }

#if defined(ADIOS_USE_USB_MIDI)
    // Say it again, about once a second, for as long as we are waiting.
    //
    // Announcing once is not enough. Whoever asked for this reset has to
    // find the port again first - it is a NEW device to the host, and the
    // tool at the other end only reopens it when it notices - so a single
    // announcement, sent the moment we are ready, goes out before anyone is
    // listening. Repeating costs nine bytes a second and removes the race
    // entirely: whenever the other end arrives, the next one is along
    // shortly.
    // The FIRST one goes out the moment the port exists - the tool that asked
    // for this reset is already waiting, and its window is not infinite. The
    // old form waited a full second before saying anything, which is exactly
    // the kind of delay that expires a waiting uploader.
    if( ADIOS_USB_MIDI_CheckAvailable(0) &&
        (!usb_announced_once || cnt < usb_announced_at || (cnt - usb_announced_at) >= 10000) ) {
      usb_announced_once = 1;
      usb_announced_at = cnt;
      BSL_SYSEX_SendUploadReq(USB0);
    }
#endif
    if( no_app ) {
      // 200 mS on, 200 mS off - same cadence as the former dead-end loop, but
      // derived from the counter instead of blocking on it, so MIDI is served
      ((cnt / 2000) & 1) ? ADIOS_SOL_Set() : ADIOS_SOL_Clr();
    } else {
      const u32 pwm_period = 50;       // *100 uS -> 5 mS
      const u32 pwm_sweep_steps = 100; // * 5 mS -> 500 mS
      u32 pwm_duty = ((cnt / pwm_period) % pwm_sweep_steps) / (pwm_sweep_steps/pwm_period);
      if( (cnt % (2*pwm_period*pwm_sweep_steps)) > pwm_period*pwm_sweep_steps )
	pwm_duty = pwm_period-pwm_duty; // negative direction each 50*100 ticks
      u32 led_on = ((cnt % pwm_period) > pwm_duty) ? 1 : 0;
      led_on ? ADIOS_SOL_Set() : ADIOS_SOL_Clr();
    }

    // call periodic hook each mS (!!! important - not shorter due to timeout counters which are handled here)
    if( (cnt % 10) == 0 ) {
      ADIOS_MIDI_Periodic_mS();
    }

    // check for incoming MIDI messages - no hooks are used
    // SysEx requests will be parsed by ADIOS internally, BSL_SYSEX_Cmd() will be called
    // directly by ADIOS to enhance command set
    // Drive the USB stack. Unlike the interrupt-only driver this replaced,
    // TinyUSB does its work in a task that something has to call: an
    // application gets that from its 1 mS tick, and this bare loop is the
    // only thing here, so it calls it itself. Without this the controller is
    // clocked and configured and the pull-up is asserted, yet enumeration
    // never completes - the host sees nothing at all, and whoever was waiting
    // for this bootloader to answer waits forever.
#if defined(ADIOS_USE_USB)
    ADIOS_USB_Handler();
#endif

    ADIOS_MIDI_Receive_Handler(NULL);

    // The "no application" case leaves as soon as SOMETHING has been written
    // and ADIOS Studio has released - main() then resets, and the whole boot
    // decision is taken again from scratch. Watching the application's reset
    // vector instead was too narrow: ADIOS Studio also installs the BSL update
    // tool ABOVE the boundary and redirects a single boot to it with the entry
    // override, which is read once per startup, before this loop. The loop
    // never saw the boundary become valid - because it does not, in that flow -
    // and the bootloader stayed put, answering "BSL" when Studio expected the
    // updater to have started (2026-08-11).
  } while( (no_app && !BSL_SYSEX_UploadStartedGet()) ||
	   BSL_SYSEX_HaltStateGet() ||                        // BSL not halted due to flash write operation
	   (hold_mode_active_after_reset && (BSL_HOLD_STATE || BSL_SYSEX_SoftHoldGet()))); // held by pin, or by the software request (released by Studio's post-upload reboot query)

#if defined(ADIOS_USE_DIN_MIDI)
  // Let whatever was just queued actually LEAVE the UART before this function
  // returns - main() hands over to the application (or resets) immediately
  // after, and both wipe the USART while transmission is interrupt driven.
  // BSL_SYSEX_ReleaseHaltState() answers ADIOS Studio's post-upload reboot
  // query with an upload request exactly here, and losing it made Studio
  // report "No response from core after reboot" on an upload that had in
  // fact succeeded (2026-08-11). The former 2-second timeout hid this by
  // lingering long enough; the flush has to be explicit now.
  ADIOS_STOPWATCH_Reset();
  while( ADIOS_UART_TxBufferFree(ADIOS_MIDI_DEFAULT_PORT & 0xf) < ADIOS_UART_TX_BUFFER_SIZE &&
	 ADIOS_STOPWATCH_ValueGet() < 2000 ); // 200 mS ceiling
  ADIOS_DELAY_Wait_uS(1000); // the byte still in the shift register
#endif

#if defined(ADIOS_USE_USB)
  // Same duty for USB - and it was missing, while the DIN had its flush
  // above. ReleaseHaltState() queues its answers to ADIOS Studio's post-
  // upload reboot query; the loop condition then goes false and this
  // function returns, main() quiesces the port, and everything still in
  // the TX FIFO dies unsent. Studio waits for exactly those bytes, and
  // reports a failed reboot on an upload that succeeded. Pumping the
  // stack for a few tens of milliseconds is all it takes to let them go.
  ADIOS_STOPWATCH_Reset();
  while( ADIOS_STOPWATCH_ValueGet() < 500 ) { // 50 mS
    ADIOS_USB_Handler();
  }
#endif
}

/////////////////////////////////////////////////////////////////////////////
// Main function
/////////////////////////////////////////////////////////////////////////////
int main(void)
{
  ///////////////////////////////////////////////////////////////////////////
  // initialize system
  ///////////////////////////////////////////////////////////////////////////
  ADIOS_SYS_Init(0);
  ADIOS_DELAY_Init(0);

#ifdef BSL_LCD_PRE_RESET
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = LCD_RST_PIN;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
	LL_GPIO_Init(LCD_RST_PORT, &GPIO_InitStruct);
	ADIOS_SYS_STM_PINSET_1(LCD_RST_PORT, LCD_RST_PIN)
	ADIOS_DELAY_Wait_uS(10000);							// wait for 10ms
#endif


  ADIOS_SOL_Init();

  // The hold pin: input, pulled DOWN, read as active-high - on EVERY family.
  // F4 used to be excluded here (a leftover of the F10x-style init), which
  // left the pin FLOATING on this family: no mode, no pull, measured reading
  // random levels. A floating hold pin means random bootloader entries and a
  // hold release that depends on the weather.
#if !defined(ADIOS_FAMILY_STM32F10x)
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
  //ADIOS_SOL_Set();
  ADIOS_STOPWATCH_Init(100); // 100 uS accuracy

  //ADIOS_STOPWATCH_Reset();
  //while( ADIOS_STOPWATCH_ValueGet() < 20000){};
  ///////////////////////////////////////////////////////////////////////////
  // get and store state of hold pin
  ///////////////////////////////////////////////////////////////////////////

  // one-shot: consumed here, right at boot, before the upload is known to
  // succeed (the physical BSL_HOLD pin remains the fallback if it doesn't)
  u8 bootloader_mode_requested = ADIOS_SYS_BootloaderModeRequested();
  u8 hold_mode_active_after_reset = BSL_HOLD_STATE | bootloader_mode_requested;
  ADIOS_SOL_Set();
  ///////////////////////////////////////////////////////////////////////////
  // initialize USB - ADIOS_USB_Init decides HOW
  ///////////////////////////////////////////////////////////////////////////
#ifndef ADIOS_DONT_USE_USB_MIDI
  // Always brought up. A LIVE session left behind by the application's
  // core-only reset is ADOPTED - the host never sees a disconnect, which is
  // the whole point of the reboot-to-BSL flow: whoever asked for the reboot
  // keeps talking on the same handles. A cold boot enumerates fresh.
  //
  // The old gate here ("only if it was already initialized") read a RAM flag
  // that every reset cleared - so it never fired - and the peripheral
  // force-reset that came with it destroyed the very session worth keeping.
  u8 usb_was_initialized = 0; // kept for BSL_WaitLoop's signature only
  ADIOS_USB_Init(0);
#else
  u8 usb_was_initialized = 0;
#endif

  ///////////////////////////////////////////////////////////////////////////
  // initialize remaining peripherals
  ///////////////////////////////////////////////////////////////////////////
  ADIOS_MIDI_Init(0); // will also initialise UART

  ///////////////////////////////////////////////////////////////////////////
  // initialize SysEx parser
  ///////////////////////////////////////////////////////////////////////////

  BSL_SYSEX_Init(0);

  // relay the software-requested hold (RTC backup flag, consumed above) into
  // the SysEx module: unlike the physical pin it has no manual release, so
  // BSL_SYSEX_ReleaseHaltState() clears it on ADIOS Studio's post-upload
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


  // NOTE: there is no "fastboot" any more, and no timed wait either. This
  // bootloader now ALWAYS hands over immediately when it has an application
  // and nothing asks it to stay - what the info block's fastboot field used
  // to switch on. Its opposite was not a feature but a 2-second delay on
  // every boot, whose only purpose was to give ADIOS Studio a chance to catch
  // a core it had just asked to reboot; the software hold flag does that
  // explicitly and reliably. The field at ADIOS_SYS_ADDR_FASTBOOT(_CONFIRM)
  // is simply no longer read - the bytes stay in the info block, and the
  // updater keeps relaying the block verbatim, so nothing has to migrate.

#ifdef BSL_LCD_PRE_RESET
	ADIOS_SYS_STM_PINSET_0(LCD_RST_PORT, LCD_RST_PIN);
	ADIOS_DELAY_Wait_uS(15000);							// wait for 10ms
	ADIOS_SYS_STM_PINSET_1(LCD_RST_PORT, LCD_RST_PIN);
#endif

  ///////////////////////////////////////////////////////////////////////////
  // 1) asked to stay? (BSL_HOLD pin, or the software request relayed above)
  //    Serve MIDI until whoever holds us lets go, then carry on to 2).
  ///////////////////////////////////////////////////////////////////////////
  if( hold_mode_active_after_reset )
    BSL_WaitLoop(hold_mode_active_after_reset, usb_was_initialized, 0);

#if defined(ADIOS_FAMILY_STM32F10x) || defined(ADIOS_FAMILY_STM32F4xx)
  // ensure that flash write access is locked
  LL_FLASH_Lock();
#endif

  // turn off LED
  ADIOS_SOL_Clr();

  ///////////////////////////////////////////////////////////////////////////
  // 2) valid application? Hand over. Otherwise fall through to the permanent
  //    service loop at the end of this function.
  ///////////////////////////////////////////////////////////////////////////
  // branch to application if reset vector is valid (should be inside flash range)
#if defined(ADIOS_FAMILY_STM32F10x) || defined(ADIOS_FAMILY_STM32F4xx) || defined(ADIOS_FAMILY_STM32G0xx)
#if defined (ADIOS_FAMILY_STM32G0xx) || defined(ADIOS_FAMILY_STM32F4xx)
  // application entry: normally the vector table right at the app/bootloader
  // boundary. The one-shot entry override (backup register, set via SysEx
  // command 0x03 - see ADIOS_SYS_AppEntryOverrideGet) redirects a SINGLE
  // boot to an alternate vector table: the one-click BSL update flow uses it
  // to hand control to an updater linked above the normal app origin.
  // Consumed (cleared) here whatever happens: if the alternate target
  // crashes, the next reset falls back to the normal boundary entry instead
  // of looping on a broken override.
  u32 app_vector_base = 0x08000000 + ADIOS_APP_FLASH_START_ADDR;
  {
    u32 entry_override = ADIOS_SYS_AppEntryOverrideGet();
    if( (entry_override >> 24) == 0x08 && !(entry_override & 3) )
      app_vector_base = entry_override;
  }
  u32 *reset_vector = (u32 *)(app_vector_base + 4);
#else
  u32 *reset_vector = (u32 *)0x08004004;
#endif
  if( (*reset_vector >> 24) == 0x08 ) {
    // reset all peripherals
#if defined(ADIOS_FAMILY_STM32F4xx)
	  LL_AHB1_GRP1_ForceReset(0xfffffffe);		// don't reset GPIOA due to USB pins
	  LL_AHB2_GRP1_ForceReset(0xffffff7f);  	// don't reset OTG_FS, so that the connectuion can survive
	  LL_APB1_GRP1_ForceReset(0xffffffff);
	  LL_APB2_GRP1_ForceReset(0xffffffff);
	  LL_AHB1_GRP1_ReleaseReset(0xffffffff);
	  LL_AHB2_GRP1_ReleaseReset(0xffffffff);
	  LL_APB1_GRP1_ReleaseReset(0xffffffff);
	  LL_APB2_GRP1_ReleaseReset(0xffffffff);
#elif defined(ADIOS_FAMILY_STM32G0xx)
	  LL_AHB1_GRP1_ForceReset(LL_AHB1_GRP1_PERIPH_ALL);		// don't reset GPIOA due to USB pins
	  LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_ALL);
	  LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_ALL);
	  LL_AHB1_GRP1_ReleaseReset(LL_AHB1_GRP1_PERIPH_ALL);
	  LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_ALL);
	  LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_ALL);
#endif

    // Hand the USB session over ALIVE. The application adopts it (see
    // port_start in adios_usb.c): the host never sees a disconnect, its
    // handles survive, and the post-upload query lands on the same port it
    // used all along. Only the interrupt is silenced - between this jump and
    // the application's adoption there is no software to serve it, and the
    // hardware NAKs the host by itself meanwhile, which is lossless.
    //
    // Taking the port DOWN here (the previous behaviour) is exactly what
    // forced every upload to end in a disconnect the host tooling then had
    // to survive.
#if defined(ADIOS_USE_USB_MIDI)
    if( ADIOS_USB_IsInitialized() )
      ADIOS_USB_HandoffPrepare();
#endif
#if defined (ADIOS_FAMILY_STM32G0xx) || defined(ADIOS_FAMILY_STM32F4xx)
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

  // No valid application (reset vector not pointing into flash). This is
  // EXACTLY the state a power cut during an upload leaves behind, so it must
  // stay recoverable over MIDI: the former code blinked the LED forever
  // without ever calling ADIOS_MIDI_Receive_Handler(), which made MIOS
  // Studio see a dead core and left the BSL_HOLD strap as the only way out.
  BSL_WaitLoop(hold_mode_active_after_reset, usb_was_initialized, 1);

  // ...and it returned, so an upload just wrote a valid application. Reset
  // rather than jump from here: the boot path above already knows how to
  // hand over (stack pointer, entry override, peripheral reset), and a fresh
  // start is also what ADIOS Studio asked for with the reboot query that
  // released the hold. Everything queued for it has been flushed by the loop.
  ADIOS_SYS_Reset();

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

#if defined(ADIOS_FAMILY_STM32F10x) || defined(ADIOS_FAMILY_STM32F4xx)
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
    ADIOS_SYS_STM_PINSET_1(srio_sclk_pin[i].port, srio_sclk_pin[i].pin_mask); // SCLK=1 by default
    GPIO_InitStructure.Pin = srio_sclk_pin[i].pin_mask;
    LL_GPIO_Init(srio_sclk_pin[i].port, &GPIO_InitStructure);
  }

  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i) {
    ADIOS_SYS_STM_PINSET_1(srio_rclk_pin[i].port, srio_rclk_pin[i].pin_mask); // RCLK=1 by default
    GPIO_InitStructure.Pin = srio_rclk_pin[i].pin_mask;
    LL_GPIO_Init(srio_rclk_pin[i].port, &GPIO_InitStructure);
  }

  for(i=0; i<SRIO_NUM_MOSI_PINS; ++i) {
    ADIOS_SYS_STM_PINSET_0(srio_mosi_pin[i].port, srio_mosi_pin[i].pin_mask); // MOSI=0 by default
    GPIO_InitStructure.Pin = srio_mosi_pin[i].pin_mask;
    LL_GPIO_Init(srio_mosi_pin[i].port, &GPIO_InitStructure);
  }

	// (a board with a MEMS microphone sharing an SPI line has to silence it
	// here before the 128 priming clocks below; none of the boards in this
	// tree does, so nothing to do.)

  // send 128 clocks to all SPI ports
  int cycle;
  for(cycle=0; cycle<128; ++cycle) {
    for(i=0; i<SRIO_NUM_SCLK_PINS; ++i) {
      ADIOS_SYS_STM_PINSET_0(srio_sclk_pin[i].port, srio_sclk_pin[i].pin_mask); // SCLK=0
    }

    ADIOS_DELAY_Wait_uS(1);

    for(i=0; i<SRIO_NUM_SCLK_PINS; ++i) {
      ADIOS_SYS_STM_PINSET_1(srio_sclk_pin[i].port, srio_sclk_pin[i].pin_mask); // SCLK=1
    }

    ADIOS_DELAY_Wait_uS(1);
  }

  // latch values
  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i) {
    ADIOS_SYS_STM_PINSET_0(srio_rclk_pin[i].port, srio_rclk_pin[i].pin_mask); // RCLK=0
  }

  ADIOS_DELAY_Wait_uS(1);

  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i) {
    ADIOS_SYS_STM_PINSET_1(srio_rclk_pin[i].port, srio_rclk_pin[i].pin_mask); // RCLK=1
  }

  ADIOS_DELAY_Wait_uS(1);

  // switch back driver modes to input with pull-up enabled
#if defined(ADIOS_FAMILY_STM32F10x)
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
    ADIOS_SYS_LPC_PINSET(srio_sclk_pin[i].port, srio_sclk_pin[i].pin, 1); // SCLK=1 by default
    ADIOS_SYS_LPC_PINSEL(srio_sclk_pin[i].port, srio_sclk_pin[i].pin, 0); // select GPIO
    ADIOS_SYS_LPC_PINDIR(srio_sclk_pin[i].port, srio_sclk_pin[i].pin, 1); // enable output driver
  }

  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i) {
    ADIOS_SYS_LPC_PINSET(srio_rclk_pin[i].port, srio_rclk_pin[i].pin, 1); // RCLK=1 by default
    ADIOS_SYS_LPC_PINSEL(srio_rclk_pin[i].port, srio_rclk_pin[i].pin, 0); // select GPIO
    ADIOS_SYS_LPC_PINDIR(srio_rclk_pin[i].port, srio_rclk_pin[i].pin, 1); // enable output driver
  }

  for(i=0; i<SRIO_NUM_MOSI_PINS; ++i) {
    ADIOS_SYS_LPC_PINSET(srio_mosi_pin[i].port, srio_mosi_pin[i].pin, 0); // MOSI=0 by default
    ADIOS_SYS_LPC_PINSEL(srio_mosi_pin[i].port, srio_mosi_pin[i].pin, 0); // select GPIO
    ADIOS_SYS_LPC_PINDIR(srio_mosi_pin[i].port, srio_mosi_pin[i].pin, 1); // enable output driver
  }

  // send 128 clocks to all SPI ports
  int cycle;
  for(cycle=0; cycle<128; ++cycle) {
    for(i=0; i<SRIO_NUM_SCLK_PINS; ++i)
      ADIOS_SYS_LPC_PINSET(srio_sclk_pin[i].port, srio_sclk_pin[i].pin, 0); // SCLK=0

    ADIOS_DELAY_Wait_uS(1);

    for(i=0; i<SRIO_NUM_SCLK_PINS; ++i)
      ADIOS_SYS_LPC_PINSET(srio_sclk_pin[i].port, srio_sclk_pin[i].pin, 1); // SCLK=1

    ADIOS_DELAY_Wait_uS(1);
  }

  // latch values
  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i)
    ADIOS_SYS_LPC_PINSET(srio_rclk_pin[i].port, srio_rclk_pin[i].pin, 0); // RCLK=0

  ADIOS_DELAY_Wait_uS(1);

  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i)
    ADIOS_SYS_LPC_PINSET(srio_rclk_pin[i].port, srio_rclk_pin[i].pin, 1); // RCLK=1

  ADIOS_DELAY_Wait_uS(1);

  // switch back driver modes to input with pull-up enabled

  // init GPIO driver modes
  for(i=0; i<SRIO_NUM_SCLK_PINS; ++i)
    ADIOS_SYS_LPC_PINDIR(srio_sclk_pin[i].port, srio_sclk_pin[i].pin, 0); // disable output driver

  for(i=0; i<SRIO_NUM_RCLK_PINS; ++i)
    ADIOS_SYS_LPC_PINDIR(srio_rclk_pin[i].port, srio_rclk_pin[i].pin, 0); // disable output driver

  for(i=0; i<SRIO_NUM_MOSI_PINS; ++i)
    ADIOS_SYS_LPC_PINDIR(srio_mosi_pin[i].port, srio_mosi_pin[i].pin, 0); // disable output driver

  return 0; // no error
#endif

#endif
}
