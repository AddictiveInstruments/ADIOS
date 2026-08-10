// $Id: mios32_sys.c 1965 2014-03-02 14:04:00Z tk $
//! \defgroup MIOS32_SYS
//!
//! System Initialisation for MIOS32
//! 
//! \{
/* ==========================================================================
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

#include <mios32.h>



#if MIOS32_APP_USE_FREERTOS
#include <FreeRTOS.h>
#include <portmacro.h>
#endif

// this module is indispensable (the CPU can't run without it - clock, vector
// table, timebase) - always compiled, no on/off toggle.

// specified in .ld file
extern u32 mios32_sys_isr_vector;
// requitred by stm32f4xx_ll_rcc
uint32_t SystemCoreClock = 168000000;
const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8]  = {0, 0, 0, 0, 1, 2, 3, 4};

/////////////////////////////////////////////////////////////////////////////
// Local Macros
/////////////////////////////////////////////////////////////////////////////

#define MEM32(addr) (*((volatile u32 *)(addr)))
#define MEM16(addr) (*((volatile u16 *)(addr)))
#define MEM8(addr)  (*((volatile u8  *)(addr)))

// Clock configuration - override-able per-project from mios32_config.h,
// without touching this file:
//   - default: HSI (internal 16 MHz RC, no crystal needed) -> PLL -> 168 MHz,
//     the simplest/fastest config for this chip. HSI is less precise than a
//     crystal (~1%, more over temperature) - fine for UART MIDI, but a
//     project relying on tight USB timing may want the crystal instead.
//   - define MIOS32_SYS_CLOCK_SOURCE_HSE in mios32_config.h to switch to the
//     external 8 MHz crystal (MBHP_CORE_STM32F4) instead.
// PLL_M and PLL_P stay at /8 and /2 for both sources here (only PLL_N and the
// source differ) - if you need different values, note that LL_RCC_PLL_
// ConfigDomain_SYS() takes named enums (LL_RCC_PLLM_DIV_x / LL_RCC_PLLP_DIV_x
// per divider value), not raw integers, so PLL_M/PLL_P can't be simple
// overridable integers the way PLL_N is.
#ifndef MIOS32_SYS_CLOCK_SOURCE_HSE
# ifndef PLL_N
#  define PLL_N 168 // HSI 16MHz / PLL_M(8) = 2MHz VCO input * 168 = 336MHz VCO / PLL_P(2) = 168MHz SYSCLK
# endif
#else
# ifndef PLL_N
#  define PLL_N 336 // HSE 8MHz / PLL_M(8) = 1MHz VCO input * 336 = 336MHz VCO / PLL_P(2) = 168MHz SYSCLK
# endif
# ifndef HSE_STARTUP_TIMEOUT
#  define HSE_STARTUP_TIMEOUT   ((uint16_t)0x0500) /*!< Time out for HSE start up */
# endif
#endif

/////////////////////////////////////////////////////////////////////////////
//! Initializes the System for MIOS32:<BR>
//! <UL>
//!   <LI>enables clock for IO ports
//!   <LI>configures pull-ups for all IO pins
//!   <LI>initializes PLL for 72 MHz @ 12 MHz ext. clock<BR>
//!       (skipped if PLL already running - relevant for proper software 
//!        reset, e.g. so that USB connection can survive)
//!   <LI>sets base address of vector table
//!   <LI>configures the suspend flags in DBGMCU_CR as specified in 
//!       MIOS32_SYS_STM32_DBGMCU_CR (can be overruled in mios32_config.h)
//!       to simplify debugging via JTAG
//!   <LI>enables the system realtime clock via MIOS32_SYS_Time_Init(0)
//! </UL>
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SYS_Init(u32 mode)
{
  // currently only mode 0 supported
  if( mode != 0 )
    return -1; // unsupported mode

  // Enable GPIOA/B/C/D clocks for ALL projects, unconditionally, here -
  // modules/drivers that use a specific GPIO port (mios32_spi.c, board-level
  // driver code, etc.) do NOT need to enable its clock themselves, it's
  // already done.
  LL_AHB1_GRP1_EnableClock(
		  LL_AHB1_GRP1_PERIPH_GPIOA |
		  LL_AHB1_GRP1_PERIPH_GPIOB |
		  LL_AHB1_GRP1_PERIPH_GPIOC |
		  LL_AHB1_GRP1_PERIPH_GPIOD);

  /* FPU settings ------------------------------------------------------------*/
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  /* set CP10 and CP11 Full Access */
#endif

    // init clock system if chip doesn't already run with PLL (skipped on a
    // software reset, so e.g. a USB connection can survive)
    __IO uint32_t HSEStatus = 0;

    if( (RCC->CFGR & (uint32_t)RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL ) {
      HSEStatus = SUCCESS;
    } else {
      LL_FLASH_SetLatency(LL_FLASH_LATENCY_5);
      while( LL_FLASH_GetLatency() != LL_FLASH_LATENCY_5 ) {}
      LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);

#ifndef MIOS32_SYS_CLOCK_SOURCE_HSE
      LL_RCC_HSI_Enable();
      while( LL_RCC_HSI_IsReady() != 1 ) {}
      LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_8, PLL_N, LL_RCC_PLLP_DIV_2);
      LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_8, PLL_N, LL_RCC_PLLQ_DIV_7);
      HSEStatus = SUCCESS;
#else
      LL_RCC_HSE_Enable();
      __IO uint32_t StartUpCounter = 0;
      do {
	HSEStatus = LL_RCC_HSE_IsReady();
	StartUpCounter++;
      } while( (HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT) );
      HSEStatus = LL_RCC_HSE_IsReady() ? SUCCESS : 0;

      if( HSEStatus == SUCCESS ) {
	LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_8, PLL_N, LL_RCC_PLLP_DIV_2);
	LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_8, PLL_N, LL_RCC_PLLQ_DIV_7);
      }
      // if HSE fails to start-up, the application will have wrong clock
      // configuration - add error handling here if this project needs it
#endif

      if( HSEStatus == SUCCESS ) {
	LL_RCC_PLL_Enable();
	while( LL_RCC_PLL_IsReady() != 1 ) {}

	LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
	LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
	LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
	while( LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL ) {}

	LL_SetSystemCoreClock(168000000);
      }
    }
  // Set the Vector Table base address as specified in .ld file (-> mios32_sys_isr_vector)
  SCB->VTOR = (u32)&mios32_sys_isr_vector | (0 & (uint32_t)0x1FFFFF80);
  NVIC_SetPriorityGrouping(MIOS32_IRQ_PRIGROUP);

#ifndef MIOS32_SYS_DONT_INIT_RTC
  // initialize system clock
  mios32_sys_time_t t = { .seconds=0, .fraction_ms=0 };
  MIOS32_SYS_TimeSet(t);
#endif

  // error during clock configuration?
  return HSEStatus == SUCCESS ? 0 : -1;
}


/////////////////////////////////////////////////////////////////////////////
//! Shutdown MIOS32 and reset the microcontroller:<BR>
//! <UL>
//!   <LI>disable all RTOS tasks
//!   <LI>print "Bootloader Mode " message if LCD enabled (since MIOS32 will enter this mode after reset)
//!   <LI>wait until all MIDI OUT buffers are empty (TODO)
//!   <LI>disable all interrupts
//!   <LI>turn off all board LEDs
//!   <LI>send all-0 to DOUT chain (TODO)
//!   <LI>send all-0 to MF control chain (if enabled) (TODO)
//!   <LI>reset STM32
//! </UL>
//! \return < 0 if reset failed
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SYS_Reset(void)
{
  // disable all RTOS tasks
#if MIOS32_APP_USE_FREERTOS
  portENTER_CRITICAL(); // port specific FreeRTOS function to disable tasks (nested)
#endif

  // NOTE: the historical "print 'Bootloader Mode' on the LCD" block was
  // removed here (2026-08-09): with the scheduler frozen by the critical
  // section above, calling into a display driver is unsafe - another task may
  // be mid-transfer on the same bus (SPI DMA), and the driver call then
  // operates on a half-configured channel. The message was visible for ~50 mS
  // before the reset anyway.

  // disable all interrupts
  MIOS32_IRQ_Disable();

  // turn off all board LEDs
  MIOS32_BOARD_LED_Set(0xffffffff, 0x00000000);

  // wait for 50 mS to ensure that all ongoing operations (e.g. DMA driver SPI transfers) are finished
  {
    int i;
    for(i=0; i<50; ++i)
      MIOS32_DELAY_Wait_uS(1000);
  }

  // reset peripherals
  LL_AHB1_GRP1_ForceReset(0xfffffffe);		// don't reset GPIOA due to USB pins
  LL_AHB2_GRP1_ForceReset(0xffffff7f);  	// don't reset OTG_FS, so that the connectuion can survive
  LL_APB1_GRP1_ForceReset(0xffffffff);
  LL_APB2_GRP1_ForceReset(0xffffffff);
  LL_AHB1_GRP1_ReleaseReset(0xffffffff);
  LL_AHB2_GRP1_ReleaseReset(0xffffffff);
  LL_APB1_GRP1_ReleaseReset(0xffffffff);
  LL_APB2_GRP1_ReleaseReset(0xffffffff);

  // CAUTION: do not replace this with a direct write to SCB->AIRCR's
  // VECTRESET bit - that bit only exists on Cortex-M0/M0+ (ARMv6-M) and is
  // reserved (no effect) on this chip's Cortex-M4 (ARMv7-M), so the CPU
  // would never actually reset. NVIC_SystemReset() (SYSRESETREQ) is the
  // correct, portable CMSIS call - used the same way on STM32G0xx.
  NVIC_SystemReset();

  while( 1 );

  return -1; // we will never reach this point
}


// The four helpers below exist only to hand a bootloader something across a
// reset: the "stay resident" request and the one-shot entry override, both
// held in TAMP/RTC backup registers. With MIOS32_USE_BOOTLOADER = 0 there is
// nothing on the other side of the reset to read them, so they are dead
// weight - and the backup-register clocking they enable is pointless too.
#if MIOS32_USE_BOOTLOADER
/////////////////////////////////////////////////////////////////////////////
//! Requests that the bootloader stays resident after the next reset, so
//! that an application can trigger a firmware update (e.g. via MIDI SysEx)
//! without requiring the user to touch the physical BSL_HOLD pin.
//! The request is stored in an RTC backup register, which survives
//! NVIC_SystemReset() (unlike RAM) and doesn't wear out flash.
//! \return 0 (no error)
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SYS_BootloaderModeRequest(void)
{
  // unlike STM32G0xx (separate TAMP peripheral with an
  // LL_RTC_BKP_SetRegister(TAMP, ...) wrapper), on STM32F4xx the backup
  // registers are plain members of the RTC peripheral itself (RTC->BKP0R,
  // no LL wrapper exists for them) - only backup domain write access needs
  // enabling first, the RTC clock/calendar itself doesn't need to be running.
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  LL_PWR_EnableBkUpAccess();

  RTC->BKP0R = MIOS32_SYS_BOOTLOADER_MODE_MAGIC;

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//! Checks the bootloader mode request flag and clears it (one-shot).
//! Only meant to be called by the bootloader itself, right at boot -
//! the flag is consumed immediately, before the upload is known to succeed
//! (the physical BSL_HOLD pin remains the fallback if it doesn't).
//! \return 1 if the flag was set, 0 otherwise
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SYS_BootloaderModeRequested(void)
{
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  LL_PWR_EnableBkUpAccess();

  u32 requested = (RTC->BKP0R == MIOS32_SYS_BOOTLOADER_MODE_MAGIC);

  RTC->BKP0R = 0;

  return requested ? 1 : 0;
}

/////////////////////////////////////////////////////////////////////////////
//! Sets the application entry override (backup register BKP1R): on the next
//! reset, a new-generation bootloader jumps to the vector table at this
//! address instead of the app/bootloader boundary. Used by the one-click
//! BSL-update flow to hand control to an updater linked ABOVE the normal
//! app origin. 0 = no override (backup registers reset to 0 on power loss -
//! a safe fallback to the normal boundary entry).
//! \param[in] addr vector table address of the alternate entry (0 to clear)
//! \return 0 (no error)
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SYS_AppEntryOverrideSet(u32 addr)
{
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  LL_PWR_EnableBkUpAccess();

  RTC->BKP1R = addr;

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//! Reads AND CLEARS the application entry override (one-shot, same pattern
//! as MIOS32_SYS_BootloaderModeRequested): consumed by the bootloader right
//! before its jump-to-application decision, so a crashing alternate entry
//! can't wedge the core in a reboot loop - the next reset falls back to the
//! normal boundary entry.
//! \return the stored vector table address, 0 if no override was set
/////////////////////////////////////////////////////////////////////////////
u32 MIOS32_SYS_AppEntryOverrideGet(void)
{
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  LL_PWR_EnableBkUpAccess();

  u32 addr = RTC->BKP1R;

  RTC->BKP1R = 0;

  return addr;
}
#endif /* MIOS32_USE_BOOTLOADER */


/////////////////////////////////////////////////////////////////////////////
//! Returns the Chip ID of the core
//! \return the chip ID
/////////////////////////////////////////////////////////////////////////////
u32 MIOS32_SYS_ChipIDGet(void)
{
  // stored in DBGMCU_IDCODE register
  return MEM32(0xe0042000);
}

/////////////////////////////////////////////////////////////////////////////
//! Returns the Flash size of the core
//! \return the Flash size in bytes
/////////////////////////////////////////////////////////////////////////////
u32 MIOS32_SYS_FlashSizeGet(void)
{
  // stored in the so called "electronic signature"
  return (u32)MEM16(0x1fff7a22) * 0x400;
}

/////////////////////////////////////////////////////////////////////////////
//! Returns the (data) RAM size of the core
//! \return the RAM size in bytes
/////////////////////////////////////////////////////////////////////////////
// exposed by the linker script (PROVIDE(_ram_size = LENGTH(RAM))) - not a
// real address, an absolute symbol whose "address" IS the RAM size in bytes
// (same idiom already used for mios32_sys_isr_vector above)
extern u32 _ram_size;

u32 MIOS32_SYS_RAMSizeGet(void)
{
  // no hardware register for this on STM32 (unlike FLASH size, which has an
  // electronic signature register) - read from the linker script instead,
  // works for any processor without a per-processor #elif chain here
  return (u32)&_ram_size;
}

/////////////////////////////////////////////////////////////////////////////
//! Returns the serial number as a string
//! \param[out] str pointer to a string which can store at least 32 digits + zero terminator!
//! (24 digits returned for STM32)
//! \return < 0 if feature not supported
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SYS_SerialNumberGet(char *str)
{
  int i;

  // stored in the so called "electronic signature"
  for(i=0; i<24; ++i) {
    u8 b = MEM8(0x1fff7a10 + (i/2));
    if( !(i & 1) )
      b >>= 4;
    b &= 0x0f;

    str[i] = ((b > 9) ? ('A'-10) : '0') + b;
  }
  str[i] = 0;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Initializes/Resets the System Real Time Clock, so that MIOS32_SYS_Time() can
//! be used for microsecond accurate measurements.
//!
//! The time can be re-initialized the following way:
//! \code
//!   // set System Time to one hour and 30 minutes
//!   mios32_sys_time_t t = { .seconds=1*3600 + 30*60, .fraction_ms=0 };
//!   MIOS32_SYS_TimeSet(t);
//! \endcode
//!
//! After system reset it will always start with 0. A battery backup option is
//! not supported by MIOS32
//!
//! \param[in] t the time in seconds + fraction part (mS)<BR>
//! Note that this format isn't completely compatible to the NTP timestamp format,
//! as the fraction has only mS accuracy
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SYS_TimeSet(mios32_sys_time_t t)
{
  // taken from STM32 example "RTC/Calendar"

  // Enable PWR and BKP clocks
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

  // Allow access to BKP Domain
  LL_PWR_EnableBkUpAccess();

  // RTC clock source - this domain is independent of the SYSCLK source
  // chosen in MIOS32_SYS_Init() above (backup domain keeps its own clocking
  // regardless of a SYSCLK change/reconfiguration). Defaults to the internal
  // LSI (~32kHz RC, no crystal needed), matching the same approach already
  // used on STM32G0xx - override to HSE/16 per-project by defining
  // MIOS32_SYS_RTC_CLOCK_SOURCE_HSE in mios32_config.h if tighter RTC
  // accuracy is needed (requires HSE actually running - see
  // MIOS32_SYS_CLOCK_SOURCE_HSE).
#ifndef MIOS32_SYS_RTC_CLOCK_SOURCE_HSE
  LL_RCC_LSI_Enable();
  while( LL_RCC_LSI_IsReady() != 1 ) {}
  LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSI);
#else
  LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_HSE);
#endif

  // Enable RTC Clock
  LL_RCC_EnableRTC();

  // initialize RTC
  LL_RTC_InitTypeDef  RTC_InitStruct;
  LL_RTC_StructInit(&RTC_InitStruct);

  // Set RTC prescaler: (AsynchPrescaler+1)*(SynchPrescaler+1) should match
  // the RTC clock frequency for a ~1Hz tick - 127*255=32385 matches LSI's
  // ~32kHz (same values already used on STM32G0xx). Override both together
  // via MIOS32_SYS_RTC_ASYNCH_PRESCALER/MIOS32_SYS_RTC_SYNCH_PRESCALER if
  // using MIOS32_SYS_RTC_CLOCK_SOURCE_HSE with a different crystal.
#ifndef MIOS32_SYS_RTC_ASYNCH_PRESCALER
# define MIOS32_SYS_RTC_ASYNCH_PRESCALER 127
#endif
#ifndef MIOS32_SYS_RTC_SYNCH_PRESCALER
# define MIOS32_SYS_RTC_SYNCH_PRESCALER 255
#endif
  RTC_InitStruct.AsynchPrescaler = MIOS32_SYS_RTC_ASYNCH_PRESCALER - 1; // 7bit maximum
  RTC_InitStruct.SynchPrescaler = MIOS32_SYS_RTC_SYNCH_PRESCALER - 1; // 13bit maximum
  LL_RTC_Init(RTC, &RTC_InitStruct);

  // Change the current time
  LL_RTC_TimeTypeDef  RTC_TimeStruct;
  LL_RTC_TIME_StructInit(&RTC_TimeStruct);
  RTC_TimeStruct.Hours = t.seconds / 3600;
  RTC_TimeStruct.Minutes = (t.seconds % 3600) / 60;
  RTC_TimeStruct.Seconds = t.seconds % 60;
  LL_RTC_TIME_Init(RTC, LL_RTC_FORMAT_BIN, &RTC_TimeStruct);
  // (fraction not taken into account here)

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Returns the System Real Time (with mS accuracy)
//!
//! Following example code converts the returned time into hours, minutes,
//! seconds and milliseconds:
//! \code
//!   mios32_sys_time_t t = MIOS32_SYS_TimeGet();
//!   int hours = t.seconds / 3600;
//!   int minutes = (t.seconds % 3600) / 60;
//!   int seconds = (t.seconds % 3600) % 60;
//!   int milliseconds = t.fraction_ms;
//! \endcode
//! \return the system time in a mios32_sys_time_t structure
/////////////////////////////////////////////////////////////////////////////
mios32_sys_time_t MIOS32_SYS_TimeGet(void)
{
  mios32_sys_time_t t = {
    .seconds = LL_RTC_TIME_Get(RTC),
    .fraction_ms = 0 // not supported
  };
  return t;
}


/////////////////////////////////////////////////////////////////////////////
//! Installs a DMA callback function which is invoked on DMA interrupts\n
//! Available for LTC17xx (and not STM32) since it only provides a single DMA
//! interrupt which is shared by all channels.
//! \param[in] dma the DMA number (currently always 0)
//! \param[in] chn the DMA channel (0..7)
//! \param[in] callback the callback function which will be invoked by DMA ISR
//! \return -1 if function not implemented for this MIOS32_PROCESSOR
//! \return -2 if invalid DMA number is selected
//! \return -2 if invalid DMA channel selected
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SYS_DMA_CallbackSet(u8 dma, u8 chn, void *callback)
{
  return -1; // function not implemented for this MIOS32_PROCESSOR
}

//! \}
