//! \defgroup ADIOS_SYS
//!
//! System Initialisation for ADIOS
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



#if ADIOS_APP_USE_FREERTOS
#include <FreeRTOS.h>
#include <portmacro.h>
#endif

// this module is indispensable (the CPU can't run without it - clock, vector
// table, timebase) - always compiled, no on/off toggle.

// specified in .ld file
extern u32 adios_sys_isr_vector;
// requitred by stm32f4xx_ll_rcc
uint32_t SystemCoreClock = ADIOS_SYS_CPU_FREQUENCY;

const uint32_t AHBPrescTable[16UL] = {0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 1UL, 2UL, 3UL, 4UL, 6UL, 7UL, 8UL, 9UL};
const uint32_t APBPrescTable[8UL] =  {0UL, 0UL, 0UL, 0UL, 1UL, 2UL, 3UL, 4UL};

/////////////////////////////////////////////////////////////////////////////
// Local Macros
/////////////////////////////////////////////////////////////////////////////

#define MEM32(addr) (*((volatile u32 *)(addr)))
#define MEM16(addr) (*((volatile u16 *)(addr)))
#define MEM8(addr)  (*((volatile u8  *)(addr)))

// note: this family clocks from HSI (internal RC, no crystal) via
// LL_RCC_PLL_ConfigDomain_SYS() below with hardcoded LL enum arguments, not
// via PLL_M/N/P/Q-style constants - unlike STM32F4xx, there's currently no
// HSE/crystal code path to override into on this family.

/////////////////////////////////////////////////////////////////////////////
//! Initializes the System for ADIOS:<BR>
//! <UL>
//!   <LI>enables clock for IO ports
//!   <LI>configures pull-ups for all IO pins
//!   <LI>initializes PLL for 72 MHz @ 12 MHz ext. clock<BR>
//!       (skipped if PLL already running - relevant for proper software 
//!        reset, e.g. so that USB connection can survive)
//!   <LI>sets base address of vector table
//!   <LI>configures the suspend flags in DBGMCU_CR as specified in 
//!       ADIOS_SYS_STM32_DBGMCU_CR (can be overruled in adios_config.h)
//!       to simplify debugging via JTAG
//!   <LI>enables the system realtime clock via ADIOS_SYS_Time_Init(0)
//! </UL>
//! \param[in] mode currently only mode 0 supported
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SYS_Init(u32 mode)
{
  // currently only mode 0 supported
  if( mode != 0 )
    return -1; // unsupported mode
  /** Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral -
   * only on chips whose SYSCFG_CFGR1 register actually has the strobe bits
   * (LL_SYSCFG_DisableDBATT itself is guarded the same way in
   * stm32g0xx_ll_system.h) - found missing on STM32G030xx, which lacks
   * both bits entirely (unlike STM32G070xx, which defines
   * SYSCFG_CFGR1_UCPD1_STROBE even without a physical UCPD1 peripheral).
  */
#if defined(SYSCFG_CFGR1_UCPD1_STROBE) || defined(SYSCFG_CFGR1_UCPD2_STROBE)
  LL_SYSCFG_DisableDBATT(LL_SYSCFG_UCPD1_STROBE | LL_SYSCFG_UCPD2_STROBE);
#endif
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

  // Enable GPIOA/B/C/D/F clocks for ALL projects, unconditionally, here -
  // modules/drivers that use a specific GPIO port (adios_spi.c, board-level
  // driver code, etc.) do NOT need to enable its clock themselves, it's
  // already done. Safe even on packages that don't bond out all of these
  // ports (e.g. STM32G030K6 in TSSOP20): the peripheral register block exists
  // in silicon across the whole G0 die family regardless of package, so
  // enabling its clock is a no-op there, never a fault.
  LL_IOP_GRP1_EnableClock(
		  LL_IOP_GRP1_PERIPH_GPIOA |
		  LL_IOP_GRP1_PERIPH_GPIOB |
		  LL_IOP_GRP1_PERIPH_GPIOC |
		  LL_IOP_GRP1_PERIPH_GPIOD |
		  LL_IOP_GRP1_PERIPH_GPIOF);

  /* FPU settings ------------------------------------------------------------*/
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  /* set CP10 and CP11 Full Access */
#endif

    // init clock system if chip doesn't already run with PLL
    __IO uint32_t HSEStatus = 0;
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
    while(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_2)
    {
    }
    /* HSI configuration and activation */
    LL_RCC_HSI_Enable();
    while(LL_RCC_HSI_IsReady() != 1)
    {
    }

    /* Main PLL configuration and activation */
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_1, 8, LL_RCC_PLLR_DIV_2);
    LL_RCC_PLL_Enable();
    LL_RCC_PLL_EnableDomain_SYS();
    while(LL_RCC_PLL_IsReady() != 1)
    {
    }

    /* Set AHB prescaler*/
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);

    /* Sysclk activation on the main PLL */
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
    {
    }

    /* Set APB1 prescaler*/
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_Init1msTick(ADIOS_SYS_CPU_FREQUENCY);
    /* Update CMSIS variable (which can be updated also through SystemCoreClockUpdate function) */
    LL_SetSystemCoreClock(ADIOS_SYS_CPU_FREQUENCY);

    HSEStatus = SUCCESS;


  // Set the Vector Table base address as specified in .ld file (-> adios_sys_isr_vector)
  SCB->VTOR = (u32)&adios_sys_isr_vector | (0 & (uint32_t)0x1FFFFF80);
  NVIC_SetPriorityGrouping(ADIOS_IRQ_PRIGROUP);
  /* SysTick_IRQn interrupt configuration */
  NVIC_SetPriority(SysTick_IRQn, 3);

#ifndef ADIOS_SYS_DONT_INIT_RTC
  // initialize system clock
  adios_sys_time_t t = { .seconds=0, .fraction_ms=0 };
  ADIOS_SYS_TimeSet(t);
#endif

  // error during clock configuration?
  return HSEStatus == SUCCESS ? 0 : -1;
}


/////////////////////////////////////////////////////////////////////////////
//! Shutdown ADIOS and reset the microcontroller:<BR>
//! <UL>
//!   <LI>disable all RTOS tasks
//!   <LI>print "Bootloader Mode " message if LCD enabled (since ADIOS will enter this mode after reset)
//!   <LI>wait until all MIDI OUT buffers are empty (TODO)
//!   <LI>disable all interrupts
//!   <LI>turn off all board LEDs
//!   <LI>send all-0 to DOUT chain (TODO)
//!   <LI>send all-0 to MF control chain (if enabled) (TODO)
//!   <LI>reset STM32
//! </UL>
//! \return < 0 if reset failed
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SYS_Reset(void)
{
  // disable all RTOS tasks
#if ADIOS_APP_USE_FREERTOS
  portENTER_CRITICAL(); // port specific FreeRTOS function to disable tasks (nested)
#endif

  // NOTE: the historical "print 'Bootloader Mode' on the LCD" block was
  // removed here (2026-08-09): with the scheduler frozen by the critical
  // section above, calling into a display driver is unsafe - another task may
  // be mid-transfer on the same bus (SPI DMA), and the driver call then
  // operates on a half-configured channel. The message was visible for ~50 mS
  // before the reset anyway.

  // disable all interrupts
  ADIOS_IRQ_Disable();

  // turn the sign-of-life LED off
#ifdef ADIOS_USE_SOL
  ADIOS_SOL_Clr();
#endif

  // wait for 50 mS to ensure that all ongoing operations (e.g. DMA driver SPI transfers) are finished
  {
    int i;
    for(i=0; i<50; ++i)
      ADIOS_DELAY_Wait_uS(1000);
  }

  // reset peripherals
  LL_AHB1_GRP1_ForceReset(LL_AHB1_GRP1_PERIPH_ALL);
  LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_ALL);
  LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_ALL);
  LL_AHB1_GRP1_ReleaseReset(LL_AHB1_GRP1_PERIPH_ALL);
  LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_ALL);
  LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_ALL);

  NVIC_SystemReset();
  while( 1 );

  return -1; // we will never reach this point
}


// The four helpers below exist only to hand a bootloader something across a
// reset: the "stay resident" request and the one-shot entry override, both
// held in TAMP/RTC backup registers. With ADIOS_USE_BOOTLOADER = 0 there is
// nothing on the other side of the reset to read them, so they are dead
// weight - and the backup-register clocking they enable is pointless too.
#if ADIOS_USE_BOOTLOADER
/////////////////////////////////////////////////////////////////////////////
//! Requests that the bootloader stays resident after the next reset, so
//! that an application can trigger a firmware update (e.g. via MIDI SysEx)
//! without requiring the user to touch the physical BSL_HOLD pin.
//! The request is stored in a TAMP/RTC backup register, which survives
//! NVIC_SystemReset() (unlike RAM) and doesn't wear out flash.
//! \return 0 (no error)
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SYS_BootloaderModeRequest(void)
{
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  // RTCAPBEN gates ALL register access to the TAMP/RTC block on STM32G0
  // (RM0454) - without it the backup-register write below is silently lost.
  // Not covered by ADIOS_SYS_Init(): projects normally skip the RTC
  // entirely via ADIOS_SYS_DONT_INIT_RTC, so it must be enabled here
  // (found 2026-08-09: the request never survived into the bootloader,
  // only the physical BSL_HOLD pin path worked).
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_RTC);
  LL_PWR_EnableBkUpAccess();

  LL_RTC_BKP_SetRegister(TAMP, LL_RTC_BKP_DR0, ADIOS_SYS_BOOTLOADER_MODE_MAGIC);

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//! Checks the bootloader mode request flag and clears it (one-shot).
//! Only meant to be called by the bootloader itself, right at boot -
//! the flag is consumed immediately, before the upload is known to succeed
//! (the physical BSL_HOLD pin remains the fallback if it doesn't).
//! \return 1 if the flag was set, 0 otherwise
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SYS_BootloaderModeRequested(void)
{
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  // same RTCAPBEN requirement as ADIOS_SYS_BootloaderModeRequest() above -
  // reads are gated too, without it this would never see the magic
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_RTC);
  LL_PWR_EnableBkUpAccess();

  u32 requested = (LL_RTC_BKP_GetRegister(TAMP, LL_RTC_BKP_DR0) == ADIOS_SYS_BOOTLOADER_MODE_MAGIC);

  LL_RTC_BKP_SetRegister(TAMP, LL_RTC_BKP_DR0, 0);

  return requested ? 1 : 0;
}

/////////////////////////////////////////////////////////////////////////////
//! Sets the application entry override (backup register BKP_DR1): on the
//! next reset, a new-generation bootloader jumps to the vector table at this
//! address instead of the app/bootloader boundary. Used by the one-click
//! BSL-update flow to hand control to an updater linked ABOVE the normal
//! app origin. 0 = no override (backup registers reset to 0 on power loss -
//! a safe fallback to the normal boundary entry).
//! \param[in] addr vector table address of the alternate entry (0 to clear)
//! \return 0 (no error)
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SYS_AppEntryOverrideSet(u32 addr)
{
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  // RTCAPBEN gates all TAMP register access - same requirement as
  // ADIOS_SYS_BootloaderModeRequest() above
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_RTC);
  LL_PWR_EnableBkUpAccess();

  LL_RTC_BKP_SetRegister(TAMP, LL_RTC_BKP_DR1, addr);

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//! Reads AND CLEARS the application entry override (one-shot, same pattern
//! as ADIOS_SYS_BootloaderModeRequested): consumed by the bootloader right
//! before its jump-to-application decision, so a crashing alternate entry
//! can't wedge the core in a reboot loop - the next reset falls back to the
//! normal boundary entry.
//! \return the stored vector table address, 0 if no override was set
/////////////////////////////////////////////////////////////////////////////
u32 ADIOS_SYS_AppEntryOverrideGet(void)
{
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_RTC);
  LL_PWR_EnableBkUpAccess();

  u32 addr = LL_RTC_BKP_GetRegister(TAMP, LL_RTC_BKP_DR1);

  LL_RTC_BKP_SetRegister(TAMP, LL_RTC_BKP_DR1, 0);

  return addr;
}
#endif /* ADIOS_USE_BOOTLOADER */


/////////////////////////////////////////////////////////////////////////////
//! Returns the Chip ID of the core
//! \return the chip ID
/////////////////////////////////////////////////////////////////////////////
u32 ADIOS_SYS_ChipIDGet(void)
{
  // stored in DBGMCU_IDCODE register
  return DBG->IDCODE;
}

/////////////////////////////////////////////////////////////////////////////
//! Returns the Flash size of the core
//! \return the Flash size in bytes
/////////////////////////////////////////////////////////////////////////////
u32 ADIOS_SYS_FlashSizeGet(void)
{
  // FLASH_SIZE is derived by CMSIS from the electronic signature register
  return FLASH_SIZE;
}

/////////////////////////////////////////////////////////////////////////////
//! Returns the (data) RAM size of the core
//! \return the RAM size in bytes
/////////////////////////////////////////////////////////////////////////////
// exposed by the linker script (PROVIDE(_ram_size = LENGTH(RAM))) - not a
// real address, an absolute symbol whose "address" IS the RAM size in bytes
// (same idiom already used for adios_sys_isr_vector above)
extern u32 _ram_size;

u32 ADIOS_SYS_RAMSizeGet(void)
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
s32 ADIOS_SYS_SerialNumberGet(char *str)
{
  int i;

  // stored in the so called "electronic signature"
  for(i=0; i<24; ++i) {
    u8 b = MEM8(UID_BASE + (i/2));
    if( !(i & 1) )
      b >>= 4;
    b &= 0x0f;

    str[i] = ((b > 9) ? ('A'-10) : '0') + b;
  }
  str[i] = 0;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Initializes/Resets the System Real Time Clock, so that ADIOS_SYS_Time() can
//! be used for microsecond accurate measurements.
//!
//! The time can be re-initialized the following way:
//! \code
//!   // set System Time to one hour and 30 minutes
//!   adios_sys_time_t t = { .seconds=1*3600 + 30*60, .fraction_ms=0 };
//!   ADIOS_SYS_TimeSet(t);
//! \endcode
//!
//! After system reset it will always start with 0. A battery backup option is
//! not supported by ADIOS
//!
//! \param[in] t the time in seconds + fraction part (mS)<BR>
//! Note that this format isn't completely compatible to the NTP timestamp format,
//! as the fraction has only mS accuracy
//! \return < 0 if initialisation failed
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SYS_TimeSet(adios_sys_time_t t)
{
  // taken from STM32 example "RTC/Calendar", adapted to clock the RTC from
  // the internal LSI (~32kHz RC, no crystal needed)

  // Enable PWR and BKP clocks
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

  // Allow access to BKP Domain
  LL_PWR_EnableBkUpAccess();

  // Select LSI as RTC Clock Source
  LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSI);

  // Enable RTC Clock
  LL_RCC_EnableRTC();

  // initialize RTC
  LL_RTC_InitTypeDef  RTC_InitStruct;
  LL_RTC_StructInit(&RTC_InitStruct);

  // Set RTC prescaler: set RTC period from 2 uS to 1 S
  RTC_InitStruct.AsynchPrescaler = 127 - 1; // 7bit maximum
  RTC_InitStruct.SynchPrescaler = 255 - 1; // 13 bit maximum
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
//!   adios_sys_time_t t = ADIOS_SYS_TimeGet();
//!   int hours = t.seconds / 3600;
//!   int minutes = (t.seconds % 3600) / 60;
//!   int seconds = (t.seconds % 3600) % 60;
//!   int milliseconds = t.fraction_ms;
//! \endcode
//! \return the system time in a adios_sys_time_t structure
/////////////////////////////////////////////////////////////////////////////
adios_sys_time_t ADIOS_SYS_TimeGet(void)
{
  adios_sys_time_t t = {
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
//! \return -1 if function not implemented for this ADIOS_PROCESSOR
//! \return -2 if invalid DMA number is selected
//! \return -2 if invalid DMA channel selected
/////////////////////////////////////////////////////////////////////////////
s32 ADIOS_SYS_DMA_CallbackSet(u8 dma, u8 chn, void *callback)
{
  return -1; // function not implemented for this ADIOS_PROCESSOR
}

//! \}
