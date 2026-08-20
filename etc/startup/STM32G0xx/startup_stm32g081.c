/**
 ******************************************************************************
 * @file      startup_stm32g081.c
 * @brief     G081xx vector table and reset handler for the GCC toolchain.
 *            This module performs:
 *                - Set the initial SP
 *                - Set the initial PC == Reset_Handler,
 *                - Set the vector table entries with the exceptions ISR address,
 *                - Branches to main() (which never returns).
 *            After Reset the Cortex-M0+ processor is in Thread mode,
 *            priority is Privileged, and the Stack is set to Main.
 *
 *            Single canonical copy shared by both the bootloader
 *            (bootloader/src) and the traditional programming model
 *            (core) - see etc/startup/README
 *            equivalent note below: don't fork a per-consumer copy, both
 *            need the exact same reset sequence and vector table for this
 *            exact chip.
 ******************************************************************************
 */

/* Private typedef -----------------------------------------------------------*/
typedef void( *const intfunc )( void );

/* Private define ------------------------------------------------------------*/
/* Note: Following vector table addresses must be defined in line with linker
         configuration. */
/*!< Uncomment the following line if you need to relocate the vector table
     anywhere in Flash or Sram, else the vector table is kept at the automatic
     remap of boot address selected */
#define USER_VECT_TAB_ADDRESS

#if defined(USER_VECT_TAB_ADDRESS)
/*!< Uncomment the following line if you need to relocate your vector Table
     in Sram else user remap will be done in Flash. */
/* #define VECT_TAB_SRAM */
#if defined(VECT_TAB_SRAM)
#define VECT_TAB_BASE_ADDRESS   SRAM_BASE       /*!< Vector Table base address field.
                                                     This value must be a multiple of 0x200. */
#define VECT_TAB_OFFSET         0x00000000U     /*!< Vector Table base offset field.
                                                     This value must be a multiple of 0x200. */
#else
#define VECT_TAB_BASE_ADDRESS   FLASH_BASE      /*!< Vector Table base address field.
                                                     This value must be a multiple of 0x200. */
#define VECT_TAB_OFFSET         0x00000000U     /*!< Vector Table base offset field.
                                                     This value must be a multiple of 0x200. */
#endif /* VECT_TAB_SRAM */
#endif /* USER_VECT_TAB_ADDRESS */

#define WEAK __attribute__ ((weak))

/* Private macro -------------------------------------------------------------*/
extern unsigned long _etext;
/* start address for the initialization values of the .data section.
defined in linker script */
extern unsigned long _sidata;

/* start address for the .data section. defined in linker script */
extern unsigned long _sdata;

/* end address for the .data section. defined in linker script */
extern unsigned long _edata;

/* start address for the .bss section. defined in linker script */
extern unsigned long _sbss;

/* end address for the .bss section. defined in linker script */
extern unsigned long _ebss;

/* init value for the stack pointer. defined in linker script */
extern unsigned long _estack;

/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
void Reset_Handler(void) __attribute__((__interrupt__));

extern int main(void);
void __Init_Data(void);

/******************************************************************************
*
* Forward declaration of the default fault handlers.
*
*******************************************************************************/
void WEAK Reset_Handler(void);
void WEAK NMI_Handler						(void);
void WEAK HardFault_Handler					(void);
void WEAK SVC_Handler						(void);
void WEAK PendSV_Handler					(void);
void WEAK SysTick_Handler					(void);

/* External Interrupts */
void WEAK WWDG_IRQHandler                (void);  /* Window WatchDog */
void WEAK PVD_IRQHandler                 (void);  /* PVD through EXTI Line detect */
void WEAK RTC_TAMP_IRQHandler            (void);  /* RTC through the EXTI line */
void WEAK FLASH_IRQHandler               (void);  /* FLASH */
void WEAK RCC_IRQHandler                 (void);  /* RCC */
void WEAK EXTI0_1_IRQHandler             (void);  /* EXTI Line 0 and 1 */
void WEAK EXTI2_3_IRQHandler             (void);  /* EXTI Line 2 and 3 */
void WEAK EXTI4_15_IRQHandler            (void);  /* EXTI Line 4 to 15 */
void WEAK UCPD1_2_IRQHandler             (void);  /* UCPD1, UCPD2 */
void WEAK DMA1_Channel1_IRQHandler       (void);  /* DMA1 Channel 1 */
void WEAK DMA1_Channel2_3_IRQHandler     (void);  /* DMA1 Channel 2 and Channel 3 */
void WEAK DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler (void);  /* DMA1 Channel 4 to Channel 7, DMAMUX1 overrun */
void WEAK ADC1_COMP_IRQHandler           (void);  /* ADC1, COMP1 and COMP2 */
void WEAK TIM1_BRK_UP_TRG_COM_IRQHandler (void);  /* TIM1 Break, Update, Trigger and Commutation */
void WEAK TIM1_CC_IRQHandler             (void);  /* TIM1 Capture Compare */
void WEAK TIM2_IRQHandler                (void);  /* TIM2 */
void WEAK TIM3_IRQHandler                (void);  /* TIM3 */
void WEAK TIM6_DAC_LPTIM1_IRQHandler     (void);  /* TIM6, DAC and LPTIM1 */
void WEAK TIM7_LPTIM2_IRQHandler         (void);  /* TIM7 and LPTIM2 */
void WEAK TIM14_IRQHandler               (void);  /* TIM14 */
void WEAK TIM15_IRQHandler               (void);  /* TIM15 */
void WEAK TIM16_IRQHandler               (void);  /* TIM16 */
void WEAK TIM17_IRQHandler               (void);  /* TIM17 */
void WEAK I2C1_IRQHandler                (void);  /* I2C1 */
void WEAK I2C2_IRQHandler                (void);  /* I2C2 */
void WEAK SPI1_IRQHandler                (void);  /* SPI1 */
void WEAK SPI2_IRQHandler                (void);  /* SPI2 */
void WEAK USART1_IRQHandler              (void);  /* USART1 */
void WEAK USART2_IRQHandler              (void);  /* USART2 */
void WEAK USART3_4_LPUART1_IRQHandler    (void);  /* USART3, USART4 and LPUART1 */
void WEAK CEC_IRQHandler                 (void);  /* CEC */
void WEAK AES_RNG_IRQHandler             (void);  /* AES and RNG */



/* Private functions ---------------------------------------------------------*/
/******************************************************************************
*
* The minimal vector table for a Cortex M0+.  Note that the proper constructs
* must be placed on this to ensure that it ends up at physical address
* 0x0000.0000.
*
******************************************************************************/

/* "used" because the HARDWARE reads this table and no C code references it:
   under whole-program optimization it is the image's only retention root, and
   without the attribute the optimizer discards it before the linker script's
   KEEP(*(.isr_vector)) can claim it - the link then produces an EMPTY binary
   and reports nothing. Inert when that optimization is off. */
__attribute__ ((section(".isr_vector"), used))
void (* const g_pfnVectors[])(void) =
{
    (intfunc)((unsigned long)&_estack), /* The initial stack pointer */
    Reset_Handler,              /* Reset Handler */
    NMI_Handler,                /* NMI Handler */
    HardFault_Handler,          /* Hard Fault Handler */
    0,                          /* Reserved */
    0,                          /* Reserved */
    0,                          /* Reserved */
    0,                          /* Reserved */
    0,                          /* Reserved */
    0,                          /* Reserved */
    0,                          /* Reserved */
    SVC_Handler,                /* SVCall Handler - FreeRTOS's port.c redefines
                                    its own handler to this exact name via
                                    FreeRTOSConfig.h (#define vPortSVCHandler
                                    SVC_Handler), so this weak entry is
                                    correct for both bare-metal and FreeRTOS
                                    builds - no #if/#else needed here. */
    0,                          /* Reserved */
    0,                          /* Reserved */
    PendSV_Handler,             /* PendSV Handler - same FreeRTOSConfig.h
                                    renaming trick (xPortPendSVHandler ->
                                    PendSV_Handler). */
    SysTick_Handler,            /* SysTick Handler - same trick
                                    (xPortSysTickHandler -> SysTick_Handler). */

    /* External Interrupts */
	WWDG_IRQHandler                   ,/* Window WatchDog */
	PVD_IRQHandler                    ,/* PVD through EXTI Line detect */
	RTC_TAMP_IRQHandler               ,/* RTC through the EXTI line */
	FLASH_IRQHandler                  ,/* FLASH */
	RCC_IRQHandler                    ,/* RCC */
	EXTI0_1_IRQHandler                ,/* EXTI Line 0 and 1 */
	EXTI2_3_IRQHandler                ,/* EXTI Line 2 and 3 */
	EXTI4_15_IRQHandler               ,/* EXTI Line 4 to 15 */
	UCPD1_2_IRQHandler                ,/* UCPD1, UCPD2 */
	DMA1_Channel1_IRQHandler          ,/* DMA1 Channel 1 */
	DMA1_Channel2_3_IRQHandler        ,/* DMA1 Channel 2 and Channel 3 */
	DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler ,/* DMA1 Channel 4 to Channel 7, DMAMUX1 overrun */
	ADC1_COMP_IRQHandler              ,/* ADC1, COMP1 and COMP2 */
	TIM1_BRK_UP_TRG_COM_IRQHandler    ,/* TIM1 Break, Update, Trigger and Commutation */
	TIM1_CC_IRQHandler                ,/* TIM1 Capture Compare */
	TIM2_IRQHandler                   ,/* TIM2 */
	TIM3_IRQHandler                   ,/* TIM3 */
	TIM6_DAC_LPTIM1_IRQHandler        ,/* TIM6, DAC and LPTIM1 */
	TIM7_LPTIM2_IRQHandler            ,/* TIM7 and LPTIM2 */
	TIM14_IRQHandler                  ,/* TIM14 */
	TIM15_IRQHandler                  ,/* TIM15 */
	TIM16_IRQHandler                  ,/* TIM16 */
	TIM17_IRQHandler                  ,/* TIM17 */
	I2C1_IRQHandler                   ,/* I2C1 */
	I2C2_IRQHandler                   ,/* I2C2 */
	SPI1_IRQHandler                   ,/* SPI1 */
	SPI2_IRQHandler                   ,/* SPI2 */
	USART1_IRQHandler                 ,/* USART1 */
	USART2_IRQHandler                 ,/* USART2 */
	USART3_4_LPUART1_IRQHandler       ,/* USART3, USART4 and LPUART1 */
	CEC_IRQHandler                    ,/* CEC */
	AES_RNG_IRQHandler                ,/* AES and RNG */
};

/**
 * @brief  This is the code that gets called when the processor first
 *          starts execution following a reset event. Only the absolutely
 *          necessary set is performed, after which the application
 *          supplied main() routine is called.
 * @param  None
 * @retval : None
*/

void Reset_Handler(void)
{
  /* Initialize data and bss */
  unsigned long *pulSrc, *pulDest;

  /* Copy the data segment initializers from flash to SRAM */
  pulSrc = &_sidata;

  for(pulDest = &_sdata; pulDest < &_edata; )
  {
    *(pulDest++) = *(pulSrc++);
  }
  /* Zero fill the bss segment. */
  for(pulDest = &_sbss; pulDest < &_ebss; )
  {
    *(pulDest++) = 0;
  }

  /* Call the application's entry point.*/
  main();

  //
  // we should never reach this point
  // however, wait endless...
  //
  while( 1 );
}

// dummy for newer gcc versions
void _init()
{
}

/*******************************************************************************
*
* Provide weak aliases for each Exception handler to the Default_Handler.
* As they are weak aliases, any function with the same name will override
* this definition.
*
*******************************************************************************/
#pragma weak NMI_Handler						= Default_Handler
#pragma weak HardFault_Handler					= Default_Handler
#pragma weak SVC_Handler						= Default_Handler
#pragma weak PendSV_Handler						= Default_Handler
#pragma weak SysTick_Handler					= Default_Handler

#pragma weak WWDG_IRQHandler                = Default_Handler
#pragma weak PVD_IRQHandler                 = Default_Handler
#pragma weak RTC_TAMP_IRQHandler            = Default_Handler
#pragma weak FLASH_IRQHandler               = Default_Handler
#pragma weak RCC_IRQHandler                 = Default_Handler
#pragma weak EXTI0_1_IRQHandler             = Default_Handler
#pragma weak EXTI2_3_IRQHandler             = Default_Handler
#pragma weak EXTI4_15_IRQHandler            = Default_Handler
#pragma weak UCPD1_2_IRQHandler             = Default_Handler
#pragma weak DMA1_Channel1_IRQHandler       = Default_Handler
#pragma weak DMA1_Channel2_3_IRQHandler     = Default_Handler
#pragma weak DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler = Default_Handler
#pragma weak ADC1_COMP_IRQHandler           = Default_Handler
#pragma weak TIM1_BRK_UP_TRG_COM_IRQHandler = Default_Handler
#pragma weak TIM1_CC_IRQHandler             = Default_Handler
#pragma weak TIM2_IRQHandler                = Default_Handler
#pragma weak TIM3_IRQHandler                = Default_Handler
#pragma weak TIM6_DAC_LPTIM1_IRQHandler     = Default_Handler
#pragma weak TIM7_LPTIM2_IRQHandler         = Default_Handler
#pragma weak TIM14_IRQHandler               = Default_Handler
#pragma weak TIM15_IRQHandler               = Default_Handler
#pragma weak TIM16_IRQHandler               = Default_Handler
#pragma weak TIM17_IRQHandler               = Default_Handler
#pragma weak I2C1_IRQHandler                = Default_Handler
#pragma weak I2C2_IRQHandler                = Default_Handler
#pragma weak SPI1_IRQHandler                = Default_Handler
#pragma weak SPI2_IRQHandler                = Default_Handler
#pragma weak USART1_IRQHandler              = Default_Handler
#pragma weak USART2_IRQHandler              = Default_Handler
#pragma weak USART3_4_LPUART1_IRQHandler    = Default_Handler
#pragma weak CEC_IRQHandler                 = Default_Handler
#pragma weak AES_RNG_IRQHandler             = Default_Handler


/**
 * @brief  This is the code that gets called when the processor receives an
 *         unexpected interrupt.  This simply enters an infinite loop, preserving
 *         the system state for examination by a debugger.
 *
 * @param  None
 * @retval : None
*/

void Default_Handler(void)
{
  /* Go into an infinite loop. */
  while (1)
  {
    // TK: TODO - insert an error notification here
    // We could send a debug message via USB, but this could be critical if there is
    // an issue in the MIDI or USB handler or related application hooks
    //
    // Alternatively we could flash the On-Board LED - it's safe!
  }
}
