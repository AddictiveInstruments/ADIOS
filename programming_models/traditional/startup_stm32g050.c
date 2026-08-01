//$Id: startup_stm32g0xx.c 1795 2013-06-01 22:13:42Z tk $
/**
 ******************************************************************************
 * @file      startup_stm32g0x0.c
 * @author    MCD Application Team
 * @version   V3.0.0
 * @date      04/06/2009
 * @brief     STM32F10x High Density Devices vector table for RIDE7 toolchain. 
 *            This module performs:
 *                - Set the initial SP
 *                - Set the initial PC == Reset_Handler,
 *                - Set the vector table entries with the exceptions ISR address,
 *                - Configure external SRAM mounted on STM3210E-EVAL board
 *                  to be used as data memory (optional, to be enabled by user)
 *                - Branches to main in the C library (which eventually
 *                  calls main()).
 *            After Reset the Cortex-M3 processor is in Thread mode,
 *            priority is Privileged, and the Stack is set to Main.
 *******************************************************************************
 * @copy
 *
 * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
 * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
 * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
 * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * <h2><center>&copy; COPYRIGHT 2009 STMicroelectronics</center></h2>
 */

/* Includes ------------------------------------------------------------------*/
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

// for FreeRTOS
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler( void );

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
void WEAK WWDG_IRQHandler                   (void);  /* Window WatchDog              */
void WEAK RTC_TAMP_IRQHandler               (void);  /* RTC Wakeup through the EXTI line */
void WEAK FLASH_IRQHandler                  (void);  /* FLASH                        */                                          
void WEAK RCC_IRQHandler                    (void);  /* RCC                          */                                            
void WEAK EXTI0_1_IRQHandler                (void);  /* EXTI Line0                   */
void WEAK EXTI2_3_IRQHandler                (void);  /* EXTI Line1                   */
void WEAK EXTI4_15_IRQHandler               (void);  /* EXTI Line2                   */
void WEAK DMA1_Channel1_IRQHandler          (void);  /* DMA1 Stream 0                */
void WEAK DMA1_Channel2_3_IRQHandler        (void);  /* DMA1 Stream 1                */
void WEAK DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler (void);  /* DMA1 Stream 2                */
void WEAK ADC1_IRQHandler                   (void);  /* ADC1         */
void WEAK TIM1_BRK_UP_TRG_COM_IRQHandler    (void);  /* TIM1 Break and TIM9          */
void WEAK TIM1_CC_IRQHandler         		(void);  /* TIM1 Update and TIM10        */
void WEAK TIM3_IRQHandler     				(void);  /* TIM1 Trigger and Commutation and TIM11 */
void WEAK TIM6_IRQHandler                	(void);  /* TIM1 Capture Compare         */
void WEAK TIM7_IRQHandler                   (void);  /* TIM2                         */
void WEAK TIM14_IRQHandler                  (void);  /* TIM3                         */
void WEAK TIM15_IRQHandler                  (void);  /* TIM4                         */
void WEAK TIM16_IRQHandler                	(void);  /* I2C1 Event                   */
void WEAK TIM17_IRQHandler                	(void);  /* I2C1 Error                   */
void WEAK I2C1_IRQHandler                	(void);  /* I2C2 Event                   */
void WEAK I2C2_IRQHandler                	(void);  /* I2C2 Error                   */
void WEAK SPI1_IRQHandler                   (void);  /* SPI1                         */                   
void WEAK SPI2_IRQHandler                   (void);  /* SPI2                         */                   
void WEAK USART1_IRQHandler                 (void);  /* USART1                       */                   
void WEAK USART2_IRQHandler                 (void);  /* USART2                       */





/* Private functions ---------------------------------------------------------*/
/******************************************************************************
*
* The minimal vector table for a Cortex M3.  Note that the proper constructs
* must be placed on this to ensure that it ends up at physical address
* 0x0000.0000.
*
******************************************************************************/

__attribute__ ((section(".isr_vector")))
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
#if 1
    SVC_Handler,                /* SVCall Handler */
#else
    // for FreeRTOS
    vPortSVCHandler,
#endif
    0,                          /* Reserved */
    0,                          /* Reserved */
#if 1
    PendSV_Handler,             /* PendSV Handler */
    SysTick_Handler,            /* SysTick Handler */
#else
    // for FreeRTOS
    xPortPendSVHandler,
    xPortSysTickHandler,
#endif

    /* External Interrupts */
	WWDG_IRQHandler                   ,/* Window WatchDog              */
	0                                 ,/* reserved                     */
	RTC_TAMP_IRQHandler               ,/* RTC through the EXTI line    */
	FLASH_IRQHandler                  ,/* FLASH                        */
	RCC_IRQHandler                    ,/* RCC                          */
	EXTI0_1_IRQHandler                ,/* EXTI Line 0 and 1            */
	EXTI2_3_IRQHandler                ,/* EXTI Line 2 and 3            */
	EXTI4_15_IRQHandler               ,/* EXTI Line 4 to 15            */
	0                                 ,/* reserved                     */
	DMA1_Channel1_IRQHandler          ,/* DMA1 Channel 1               */
	DMA1_Channel2_3_IRQHandler        ,/* DMA1 Channel 2 and Channel 3 */
	DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler ,/* DMA1 Channel 4 to Channel 7, DMAMUX1 overrun */
	ADC1_IRQHandler                   ,/* ADC1                         */
	TIM1_BRK_UP_TRG_COM_IRQHandler    ,/* TIM1 Break, Update, Trigger and Commutation */
	TIM1_CC_IRQHandler                ,/* TIM1 Capture Compare         */
	0                                 ,/* reserved                     */
	TIM3_IRQHandler                   ,/* TIM3                         */
	TIM6_IRQHandler                   ,/* TIM6                         */
	TIM7_IRQHandler                   ,/* TIM7                         */
	TIM14_IRQHandler                  ,/* TIM14                        */
	TIM15_IRQHandler                  ,/* TIM15                        */
	TIM16_IRQHandler                  ,/* TIM16                        */
	TIM17_IRQHandler                  ,/* TIM17                        */
	I2C1_IRQHandler                   ,/* I2C1                         */
	I2C2_IRQHandler                   ,/* I2C2                         */
	SPI1_IRQHandler                   ,/* SPI1                         */
	SPI2_IRQHandler                   ,/* SPI2                         */
	USART1_IRQHandler                 ,/* USART1                       */
	USART2_IRQHandler                 ,/* USART2                       */
	0                                 ,/* reserved                     */
	0                                 ,/* reserved                     */
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

#pragma weak WWDG_IRQHandler                   	= Default_Handler
#pragma weak RTC_TAMP_IRQHandler               	= Default_Handler
#pragma weak FLASH_IRQHandler                  	= Default_Handler
#pragma weak RCC_IRQHandler                    	= Default_Handler
#pragma weak EXTI0_1_IRQHandler                	= Default_Handler
#pragma weak EXTI2_3_IRQHandler                	= Default_Handler
#pragma weak EXTI4_15_IRQHandler               	= Default_Handler
#pragma weak DMA1_Channel1_IRQHandler          	= Default_Handler
#pragma weak DMA1_Channel2_3_IRQHandler        	= Default_Handler
#pragma weak DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler 	= Default_Handler
#pragma weak ADC1_IRQHandler                   	= Default_Handler
#pragma weak TIM1_BRK_UP_TRG_COM_IRQHandler    	= Default_Handler
#pragma weak TIM1_CC_IRQHandler         		= Default_Handler
#pragma weak TIM3_IRQHandler     				= Default_Handler
#pragma weak TIM6_IRQHandler                	= Default_Handler
#pragma weak TIM7_IRQHandler                   	= Default_Handler
#pragma weak TIM14_IRQHandler                  	= Default_Handler
#pragma weak TIM15_IRQHandler                  	= Default_Handler
#pragma weak TIM16_IRQHandler                	= Default_Handler
#pragma weak TIM17_IRQHandler                	= Default_Handler
#pragma weak I2C1_IRQHandler                	= Default_Handler
#pragma weak I2C2_IRQHandler                	= Default_Handler
#pragma weak SPI1_IRQHandler                  	= Default_Handler
#pragma weak SPI2_IRQHandler                  	= Default_Handler
#pragma weak USART1_IRQHandler                	= Default_Handler
#pragma weak USART2_IRQHandler                 	= Default_Handler



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

/******************* (C) COPYRIGHT 2009 STMicroelectronics *****END OF FILE****/
