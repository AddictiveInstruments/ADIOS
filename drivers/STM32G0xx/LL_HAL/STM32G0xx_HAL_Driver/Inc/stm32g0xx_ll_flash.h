/**
  ******************************************************************************
  * @file    stm32f4xx_flash.h
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    30-September-2011
  * @brief   This file contains all the functions prototypes for the FLASH 
  *          firmware library.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32G0xx_FLASH_H
#define __STM32G0xx_FLASH_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx.h"

/** @addtogroup STM32F4xx_StdPeriph_Driver
  * @{
  */

/** @addtogroup FLASH
  * @{
  */ 

/* Exported types ------------------------------------------------------------*/
/** 
  * @brief FLASH Status  
  */ 
 typedef enum
 {
   FLASH_BUSY = 1,
   FLASH_ERROR,
   FLASH_COMPLETE,
   FLASH_TIMEOUT,
   FLASH_OK
 }LL_FLASH_Status;

/* Exported constants --------------------------------------------------------*/

/** @defgroup FLASH_Exported_Constants
  * @{
  */  

/** @defgroup Flash_Latency 
  * @{
  */ 
#define LL_FLASH_Latency_0                ((uint8_t)0x0000)  /*!< FLASH Zero Latency cycle */
#define LL_FLASH_Latency_1                ((uint8_t)0x0001)  /*!< FLASH One Latency cycle */
#define LL_FLASH_Latency_2                ((uint8_t)0x0002)  /*!< FLASH Two Latency cycles */

#define IS_LL_FLASH_LATENCY(LATENCY) (((LATENCY) == LL_FLASH_Latency_0) || \
                                   ((LATENCY) == LL_FLASH_Latency_1) || \
                                   ((LATENCY) == LL_FLASH_Latency_2))
/**
  * @}
  */ 

/** @defgroup FLASH_Voltage_Range 
  * @{
  */ 
#define LL_FLASH_VOLTRG_1        ((uint8_t)0x00)  /*!< Device operating range: 1.8V to 2.1V */
#define LL_FLASH_VOLTRG_2        ((uint8_t)0x01)  /*!<Device operating range: 2.1V to 2.7V */
#define LL_FLASH_VOLTRG_3        ((uint8_t)0x02)  /*!<Device operating range: 2.7V to 3.6V */
#define LL_FLASH_VOLTRG_4        ((uint8_t)0x03)  /*!<Device operating range: 2.7V to 3.6V + External Vpp */

#define IS_LL_VOLTAGERANGE(RANGE)(((RANGE) == LL_FLASH_VOLTRG_1) || \
                               ((RANGE) == LL_FLASH_VOLTRG_2) || \
                               ((RANGE) == LL_FLASH_VOLTRG_3) || \
                               ((RANGE) == LL_FLASH_VOLTRG_4))
/**
  * @}
  */ 

/** @defgroup Option_Bytes_Write_Protection 
  * @{
  */ 
#define LL_FLASH_OB_WRP_Sector_0       ((uint32_t)0x00000001) /*!< Write protection of Sector0 */
#define LL_FLASH_OB_WRP_Sector_1       ((uint32_t)0x00000002) /*!< Write protection of Sector1 */
#define LL_FLASH_OB_WRP_Sector_2       ((uint32_t)0x00000004) /*!< Write protection of Sector2 */
#define LL_FLASH_OB_WRP_Sector_3       ((uint32_t)0x00000008) /*!< Write protection of Sector3 */
#define LL_FLASH_OB_WRP_Sector_4       ((uint32_t)0x00000010) /*!< Write protection of Sector4 */
#define LL_FLASH_OB_WRP_Sector_5       ((uint32_t)0x00000020) /*!< Write protection of Sector5 */
#define LL_FLASH_OB_WRP_Sector_6       ((uint32_t)0x00000040) /*!< Write protection of Sector6 */
#define LL_FLASH_OB_WRP_Sector_7       ((uint32_t)0x00000080) /*!< Write protection of Sector7 */
#define LL_FLASH_OB_WRP_Sector_8       ((uint32_t)0x00000100) /*!< Write protection of Sector8 */
#define LL_FLASH_OB_WRP_Sector_9       ((uint32_t)0x00000200) /*!< Write protection of Sector9 */
#define LL_FLASH_OB_WRP_Sector_10      ((uint32_t)0x00000400) /*!< Write protection of Sector10 */
#define LL_FLASH_OB_WRP_Sector_11      ((uint32_t)0x00000800) /*!< Write protection of Sector11 */
#define LL_FLASH_OB_WRP_Sector_All     ((uint32_t)0x00000FFF) /*!< Write protection of all Sectors */

#define IS_LL_FLASH_OB_WRP(SECTOR)((((SECTOR) & (uint32_t)0xFFFFF000) == 0x00000000) && ((SECTOR) != 0x00000000))
/**
  * @}
  */

/** @defgroup FLASH_Option_Bytes_Read_Protection 
  * @{
  */
#define LL_FLASH_OB_RDP_Level_0   ((uint8_t)0xAA)
#define LL_FLASH_OB_RDP_Level_1   ((uint8_t)0x55)
/*#define LL_FLASH_OB_RDP_Level_2   ((uint8_t)0xCC)*/ /*!< Warning: When enabling read protection level 2
                                                  it's no more possible to go back to level 1 or 0 */
#define IS_LL_FLASH_OB_RDP(LEVEL) (((LEVEL) == LL_FLASH_OB_RDP_Level_0)||\
                          ((LEVEL) == LL_FLASH_OB_RDP_Level_1))/*||\
                          ((LEVEL) == LL_FLASH_OB_RDP_Level_2))*/
/**
  * @}
  */ 

/** @defgroup FLASH_Option_Bytes_IWatchdog 
  * @{
  */ 
#define LL_FLASH_OB_IWDG_SW                     ((uint8_t)0x20)  /*!< Software IWDG selected */
#define LL_FLASH_OB_IWDG_HW                     ((uint8_t)0x00)  /*!< Hardware IWDG selected */
#define IS_LL_FLASH_OB_IWDG_SOURCE(SOURCE) (((SOURCE) == LL_FLASH_OB_IWDG_SW) || ((SOURCE) == LL_FLASH_OB_IWDG_HW))
/**
  * @}
  */ 

/** @defgroup FLASH_Option_Bytes_nRST_STOP 
  * @{
  */ 
#define LL_FLASH_OB_STOP_NoRST                  ((uint8_t)0x40) /*!< No reset generated when entering in STOP */
#define LL_FLASH_OB_STOP_RST                    ((uint8_t)0x00) /*!< Reset generated when entering in STOP */
#define IS_LL_FLASH_OB_STOP_SOURCE(SOURCE) (((SOURCE) == LL_FLASH_OB_STOP_NoRST) || ((SOURCE) == LL_FLASH_OB_STOP_RST))
/**
  * @}
  */ 


/** @defgroup FLASH_Option_Bytes_nRST_STDBY 
  * @{
  */ 
#define LL_FLASH_OB_STDBY_NoRST                 ((uint8_t)0x80) /*!< No reset generated when entering in STANDBY */
#define LL_FLASH_OB_STDBY_RST                   ((uint8_t)0x00) /*!< Reset generated when entering in STANDBY */
#define IS_LL_FLASH_OB_STDBY_SOURCE(SOURCE) (((SOURCE) == LL_FLASH_OB_STDBY_NoRST) || ((SOURCE) == LL_FLASH_OB_STDBY_RST))
/**
  * @}
  */
  
/** @defgroup FLASH_BOR_Reset_Level 
  * @{
  */  
#define LL_FLASH_OB_BOR_LEVEL3          ((uint8_t)0x00)  /*!< Supply voltage ranges from 2.70 to 3.60 V */
#define LL_FLASH_OB_BOR_LEVEL2          ((uint8_t)0x04)  /*!< Supply voltage ranges from 2.40 to 2.70 V */
#define LL_FLASH_OB_BOR_LEVEL1          ((uint8_t)0x08)  /*!< Supply voltage ranges from 2.10 to 2.40 V */
#define LL_FLASH_OB_BOR_OFF             ((uint8_t)0x0C)  /*!< Supply voltage ranges from 1.62 to 2.10 V */
#define IS_LL_FLASH_OB_BOR(LEVEL) (((LEVEL) == LL_FLASH_OB_BOR_LEVEL1) || ((LEVEL) == LL_FLASH_OB_BOR_LEVEL2) ||\
                          ((LEVEL) == LL_FLASH_OB_BOR_LEVEL3) || ((LEVEL) == LL_FLASH_OB_BOR_OFF))
/**
  * @}
  */

/** @defgroup FLASH_Interrupts 
  * @{
  */ 
#define LL_FLASH_IT_EOP                   ((uint32_t)0x01000000)  /*!< End of FLASH Operation Interrupt source */
#define LL_FLASH_IT_ERR                   ((uint32_t)0x02000000)  /*!< Error Interrupt source */
#define IS_LL_FLASH_IT(IT) ((((IT) & (uint32_t)0xFCFFFFFF) == 0x00000000) && ((IT) != 0x00000000))
/**
  * @}
  */ 

/** @defgroup FLASH_Flags 
  * @{
  */ 
#define LL_FLASH_FLAG_EOP                 ((uint32_t)0x00000001)  /*!< FLASH End of Operation flag */
#define LL_FLASH_FLAG_OPERR               ((uint32_t)0x00000002)  /*!< FLASH operation Error flag */
#define LL_FLASH_FLAG_WRPERR              ((uint32_t)0x00000010)  /*!< FLASH Write protected error flag */
#define LL_FLASH_FLAG_PGAERR              ((uint32_t)0x00000020)  /*!< FLASH Programming Alignment error flag */
#define LL_FLASH_FLAG_PGPERR              ((uint32_t)0x00000040)  /*!< FLASH Programming Parallelism error flag  */
#define LL_FLASH_FLAG_PGSERR              ((uint32_t)0x00000080)  /*!< FLASH Programming Sequence error flag  */
#define LL_FLASH_FLAG_BSY                 ((uint32_t)0x00010000)  /*!< FLASH Busy flag */
#define IS_LL_FLASH_CLEAR_FLAG(FLAG) ((((FLAG) & (uint32_t)0xFFFFFF0C) == 0x00000000) && ((FLAG) != 0x00000000))
#define IS_LL_FLASH_GET_FLAG(FLAG)  (((FLAG) == LL_FLASH_FLAG_EOP) || ((FLAG) == LL_FLASH_FLAG_OPERR) || \
                                  ((FLAG) == LL_FLASH_FLAG_WRPERR) || ((FLAG) == LL_FLASH_FLAG_PGAERR) || \
                                  ((FLAG) == LL_FLASH_FLAG_PGPERR) || ((FLAG) == LL_FLASH_FLAG_PGSERR) || \
                                  ((FLAG) == LL_FLASH_FLAG_BSY))
/**
  * @}
  */

/** @defgroup FLASH_Type_Program FLASH Program Type
  * @{
  */
#define LL_FLASH_TYPEPROGRAM_DOUBLEWORD    FLASH_CR_PG     /*!< Program a double-word (64-bit) at a specified address */
#define LL_FLASH_TYPEPROGRAM_FAST          FLASH_CR_FSTPG  /*!< Fast program a 32 row double-word (64-bit) at a specified address */
/**
  * @}
  */

/** @defgroup FLASH_Keys 
  * @{
  */ 
#define LL_FLASH_RDP_KEY                  ((uint16_t)0x00A5)
#define LL_FLASH_KEY1               ((uint32_t)0x45670123)
#define LL_FLASH_KEY2               ((uint32_t)0xCDEF89AB)
#define LL_FLASH_OPT_KEY1           ((uint32_t)0x08192A3B)
#define LL_FLASH_OPT_KEY2           ((uint32_t)0x4C5D6E7F)
/**
  * @}
  */ 

/** 
  * @brief   ACR register byte 0 (Bits[8:0]) base address  
  */ 
#define LL_FLASH_ACR_BYTE0_ADDRESS           ((uint32_t)0x40023C00)
/** 
  * @brief   OPTCR register byte 3 (Bits[24:16]) base address  
  */ 
#define LL_FLASH_OPTCR_BYTE0_ADDRESS         ((uint32_t)0x40023C14)
#define LL_FLASH_OPTCR_BYTE1_ADDRESS         ((uint32_t)0x40023C15)
#define LL_FLASH_OPTCR_BYTE2_ADDRESS         ((uint32_t)0x40023C16)

/**
  * @}
  */ 
/* Private constants --------------------------------------------------------*/
 /** @defgroup FLASH_Banks FLASH Banks
   * @{
   */
 #define LL_FLASH_BANK_1                    FLASH_CR_MER1   /*!< Bank 1 */
 #if defined(FLASH_DBANK_SUPPORT)
 #define LL_FLASH_BANK_2                    FLASH_CR_MER2   /*!< Bank 2 */
 #endif /* FLASH_DBANK_SUPPORT */
 /**
   * @}
   */

/** @defgroup FLASH_Private_Constants FLASH Private Constants
  * @{
  */
#define LL_FLASH_SIZE_DATA_REGISTER        FLASHSIZE_BASE

#if defined(FLASH_DBANK_SUPPORT)
#define LL_OB_DUAL_BANK_BASE               (FLASH_R_BASE + 0x20U)               /*!< Not use cmsis FLASH alias to avoid iar warning about volatile reading sequence */
#define LL_FLASH_SALES_TYPE_Pos            (24U)
#define LL_FLASH_SALES_TYPE                (0x3UL << FLASH_SALES_TYPE_Pos)     /*!< 0x000001E0 */
#define LL_FLASH_SALES_TYPE_0              (0x1UL << FLASH_SALES_TYPE_Pos)     /*!< 0x01000000 */
#define LL_FLASH_SALES_TYPE_1              (0x2UL << FLASH_SALES_TYPE_Pos)     /*!< 0x02000000 */
#define LL_FLASH_SALES_VALUE               ((*((uint32_t *)PACKAGE_BASE)) & (FLASH_SALES_TYPE))
#define LL_OB_DUAL_BANK_VALUE              ((*((uint32_t *)LL_OB_DUAL_BANK_BASE)) & (FLASH_OPTR_DUAL_BANK))
#define LL_FLASH_BANK_NB                   (((LL_FLASH_SALES_VALUE == 0U)\
                                          || ((LL_FLASH_SALES_VALUE == LL_FLASH_SALES_TYPE_0) && (LL_OB_DUAL_BANK_VALUE == 0U)))?1U:2U)
#define LL_FLASH_BANK_SIZE                 ((LL_FLASH_BANK_NB==1U)?(FLASH_SIZE):(FLASH_SIZE >> 1U)) /*!< FLASH Bank Size. Divided by 2 if 2 Banks */
#else /* FLASH_DBANK_SUPPORT */
#define LL_FLASH_BANK_SIZE                 (FLASH_SIZE)   /*!< FLASH Bank Size */
#endif /* FLASH_DBANK_SUPPORT */

#define LL_FLASH_PAGE_SIZE                 0x00000800U    /*!< FLASH Page Size, 2 KBytes */
#define LL_FLASH_PAGE_NB                   (LL_FLASH_BANK_SIZE/LL_FLASH_PAGE_SIZE) /* Number of pages per bank */
#define LL_FLASH_TIMEOUT_VALUE             1000U          /*!< FLASH Execution Timeout, 1 s */
#define LL_FLASH_TYPENONE                  0x00000000U    /*!< No programming Procedure On Going */

#if defined(FLASH_PCROP_SUPPORT)
#define LL_FLASH_SR_ERRORS                 (FLASH_SR_OPERR  | FLASH_SR_PROGERR | FLASH_SR_WRPERR | \
                                         FLASH_SR_PGAERR | FLASH_SR_SIZERR  | FLASH_SR_PGSERR |  \
                                         FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR |   \
                                         FLASH_SR_OPTVERR) /*!< All SR error flags */
#else
#define LL_FLASH_SR_ERRORS                 (FLASH_SR_OPERR  | FLASH_SR_PROGERR | FLASH_SR_WRPERR | \
                                         FLASH_SR_PGAERR | FLASH_SR_SIZERR  | FLASH_SR_PGSERR |  \
                                         FLASH_SR_MISERR | FLASH_SR_FASTERR |                    \
                                         FLASH_SR_OPTVERR) /*!< All SR error flags */
#endif /* FLASH_PCROP_SUPPORT */

#if defined(FLASH_DBANK_SUPPORT)
#define LL_FLASH_SR_CLEAR                  (LL_FLASH_SR_ERRORS | FLASH_SR_EOP | FLASH_SR_PESD)
#else
#define LL_FLASH_SR_CLEAR                  (LL_FLASH_SR_ERRORS | FLASH_SR_EOP)
#endif /* FLASH_DBANK_SUPPORT */

/**
  * @}
  */

/* Private macros ------------------------------------------------------------*/
/** @defgroup FLASH_Private_Macros FLASH Private Macros
  *  @{
  */
#define IS_LL_FLASH_MAIN_MEM_ADDRESS(__ADDRESS__)         (((__ADDRESS__) >= (FLASH_BASE))\
                                                        && ((__ADDRESS__) <= (FLASH_BASE + FLASH_SIZE - 1UL)))

#define IS_LL_FLASH_MAIN_FIRSTHALF_MEM_ADDRESS(__ADDRESS__)  (((__ADDRESS__) >= (FLASH_BASE))\
                                                           && ((__ADDRESS__) <= (FLASH_BASE + FLASH_BANK_SIZE - 1UL)))
#if defined(FLASH_DBANK_SUPPORT)
#define IS_LL_FLASH_MAIN_SECONDHALF_MEM_ADDRESS(__ADDRESS__) (((__ADDRESS__) >= (FLASH_BASE + FLASH_BANK_SIZE))\
                                                           && ((__ADDRESS__) <= (FLASH_BASE + FLASH_SIZE - 1UL)))
#endif /* FLASH_DBANK_SUPPORT */

#define IS_LL_FLASH_PROGRAM_MAIN_MEM_ADDRESS(__ADDRESS__) (((__ADDRESS__) >= (FLASH_BASE))\
                                                        && ((__ADDRESS__) <= (FLASH_BASE + FLASH_SIZE - 8UL)))

#define IS_LL_FLASH_PROGRAM_OTP_ADDRESS(__ADDRESS__)      (((__ADDRESS__) >= 0x1FFF7000U)\
                                                        && ((__ADDRESS__) <= (0x1FFF7400U - 8UL)))

#define IS_LL_FLASH_PROGRAM_ADDRESS(__ADDRESS__)          ((IS_FLASH_PROGRAM_MAIN_MEM_ADDRESS(__ADDRESS__))\
                                                        || (IS_FLASH_PROGRAM_OTP_ADDRESS(__ADDRESS__)))

#define IS_LL_FLASH_FAST_PROGRAM_ADDRESS(__ADDRESS__)     (((__ADDRESS__) >= (FLASH_BASE))\
                                                        && ((__ADDRESS__) <= (FLASH_BASE + FLASH_SIZE - 256UL)))

#define IS_LL_FLASH_PAGE(__PAGE__)                        ((__PAGE__) < FLASH_PAGE_NB)

#if defined(FLASH_DBANK_SUPPORT)
#define IS_LL_FLASH_BANK(__BANK__)                       \
  ((FLASH_BANK_NB == 2U) ?                         \
   (((__BANK__) == FLASH_BANK_1)  ||               \
    ((__BANK__) == FLASH_BANK_2)  ||                \
    ((__BANK__) == (FLASH_BANK_2 | FLASH_BANK_1))): \
   ((__BANK__) == FLASH_BANK_1))
#else
#define IS_LL_FLASH_BANK(__BANK__)                        ((__BANK__) == FLASH_BANK_1)
#endif /* FLASH_DBANK_SUPPORT */

/* Exported macro ------------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/ 
 
/* FLASH Interface configuration functions ************************************/
//void LL_FLASH_SetLatency(uint32_t FLASH_Latency);
//void LL_FLASH_PrefetchBufferCmd(FunctionalState NewState);
//void LL_FLASH_InstructionCacheCmd(FunctionalState NewState);
//void LL_FLASH_DataCacheCmd(FunctionalState NewState);
//void LL_FLASH_InstructionCacheReset(void);
//void LL_FLASH_DataCacheReset(void);
void LL_FLASH_FlushCaches(void);
/* FLASH Memory Programming functions *****************************************/   
LL_FLASH_Status LL_FLASH_Unlock(void);
LL_FLASH_Status LL_FLASH_Lock(void);
LL_FLASH_Status LL_FLASH_PageErase(uint32_t Banks, uint32_t Page);
//LL_FLASH_Status LL_FLASH_EraseSector(uint32_t FLASH_Sector, uint8_t VoltageRange);
//LL_FLASH_Status LL_FLASH_EraseAllSectors(uint8_t VoltageRange);
LL_FLASH_Status LL_FLASH_ProgramDoubleWord(uint32_t Address, uint64_t Data);
//LL_FLASH_Status LL_FLASH_ProgramWord(uint32_t Address, uint32_t Data);
//LL_FLASH_Status LL_FLASH_ProgramHalfWord(uint32_t Address, uint16_t Data);
//LL_FLASH_Status LL_FLASH_ProgramByte(uint32_t Address, uint8_t Data);

/* Option Bytes Programming functions *****************************************/ 
//void LL_FLASH_OB_Unlock(void);
//void LL_FLASH_OB_Lock(void);
//void LL_FLASH_OB_WRPConfig(uint32_t OB_WRP, FunctionalState NewState);
//void LL_FLASH_OB_RDPConfig(uint8_t OB_RDP);
//void LL_FLASH_OB_UserConfig(uint8_t OB_IWDG, uint8_t OB_STOP, uint8_t OB_STDBY);
//void LL_FLASH_OB_BORConfig(uint8_t OB_BOR);
//LL_FLASH_Status LL_FLASH_OB_Launch(void);
//uint8_t LL_FLASH_OB_GetUser(void);
//uint16_t LL_FLASH_OB_GetWRP(void);
//FlagStatus LL_FLASH_OB_GetRDP(void);
//uint8_t LL_FLASH_OB_GetBOR(void);

/* Interrupts and flags management functions **********************************/
//void LL_FLASH_ITConfig(uint32_t FLASH_IT, FunctionalState NewState);
//FlagStatus LL_FLASH_GetFlagStatus(uint32_t FLASH_FLAG);
void LL_FLASH_ClearFlag(uint32_t FLASH_FLAG);
uint32_t LL_FLASH_GetError(void);
LL_FLASH_Status LL_FLASH_WaitForLastOperation(uint32_t Timeout);

#ifdef __cplusplus
}
#endif

#endif /* __STM32G0xx_FLASH_H */

/**
  * @}
  */ 

/**
  * @}
  */ 

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
