/**
  ******************************************************************************
  * @file    stm32f4xx.h
  * @author  MCD Application Team
  * @brief   CMSIS STM32F4xx Device Peripheral Access Layer Header File.
  *
  *          The file is the unique include file that the application programmer
  *          is using in the C source code, usually in main.c. This file contains:
  *           - Configuration section that allows to select:
  *              - The STM32F4xx device used in the target application
  *              - To use or not the peripheral's drivers in application code(i.e.
  *                code will be based on direct access to peripheral's registers
  *                rather than drivers API), this option is controlled by
  *                "#define USE_HAL_DRIVER"
  *
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/** @addtogroup CMSIS
  * @{
  */

/** @addtogroup stm32f4xx
  * @{
  */

#ifndef __STM32F4xx_H
#define __STM32F4xx_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** @addtogroup Library_configuration_section
  * @{
  */

/**
  * @brief STM32 Family
  */
#if !defined  (STM32F4)
#define STM32F4
#endif /* STM32F4 */

/* Device selection - NOTHING IS SELECTED HERE ANY MORE.
   The device macro (STM32F407xx, STM32F446xx, STM32F410Rx, ...) is derived
   from PROCESSOR and passed with -D by mios32/mios32.mk, exactly the way
   the .ld and startup files are derived in core.mk. This
   family is the one with irregular header names - F401/F411 split by
   density, F410/F412 by package - and the derivation rule there covers
   them; see its comment. If no device macro is defined, the #error at the
   end of the include chain below (ST's own) stops the build.

   HISTORY, kept because both failure modes were real. The original ST
   template here fell through to an UNCONDITIONAL "#define STM32F405xx"
   whenever no device macro was pre-defined - so every F4 build, whatever
   the real target, silently compiled against F405's device header.
   Confirmed 2026-08-04: the F407VE bring-up that day built and ran
   correctly on real hardware only because F405/F407/F415/F417 share a
   close-enough peripheral set for what the OS touches (GPIO/UART/basic
   timers); it would have miscompiled in silence against an F429, F446 or
   F469. The sibling family had the same class of bug in a different guise
   (see stm32g0xx.h). Both were replaced by a hand-written table, correct
   but knowing three F4 parts out of the twenty-three whose headers ship
   here. Deriving the macro removes the silent default AND the hand-list. */

/*  Tip: To avoid modifying this file each time you need to switch between these
        devices, you can define the device in your toolchain compiler preprocessor.
  */
#if !defined  (USE_HAL_DRIVER)
/**
 * @brief Comment the line below if you will not use the peripherals drivers.
   In this case, these drivers will not be included and the application code will
   be based on direct access to peripherals registers
   */
/*#define USE_HAL_DRIVER */
#endif /* USE_HAL_DRIVER */

/**
  * @brief CMSIS version number V2.6.10
  */
#define __STM32F4xx_CMSIS_VERSION_MAIN   (0x02U) /*!< [31:24] main version */
#define __STM32F4xx_CMSIS_VERSION_SUB1   (0x06U) /*!< [23:16] sub1 version */
#define __STM32F4xx_CMSIS_VERSION_SUB2   (0x0AU) /*!< [15:8]  sub2 version */
#define __STM32F4xx_CMSIS_VERSION_RC     (0x00U) /*!< [7:0]  release candidate */
#define __STM32F4xx_CMSIS_VERSION        ((__STM32F4xx_CMSIS_VERSION_MAIN << 24)\
                                         |(__STM32F4xx_CMSIS_VERSION_SUB1 << 16)\
                                         |(__STM32F4xx_CMSIS_VERSION_SUB2 << 8 )\
                                         |(__STM32F4xx_CMSIS_VERSION_RC))

/**
  * @}
  */

/** @addtogroup Device_Included
  * @{
  */

#if defined(STM32F405xx)
#include "stm32f405xx.h"
#elif defined(STM32F415xx)
#include "stm32f415xx.h"
#elif defined(STM32F407xx)
#include "stm32f407xx.h"
#elif defined(STM32F417xx)
#include "stm32f417xx.h"
#elif defined(STM32F427xx)
#include "stm32f427xx.h"
#elif defined(STM32F437xx)
#include "stm32f437xx.h"
#elif defined(STM32F429xx)
#include "stm32f429xx.h"
#elif defined(STM32F439xx)
#include "stm32f439xx.h"
#elif defined(STM32F401xC)
#include "stm32f401xc.h"
#elif defined(STM32F401xE)
#include "stm32f401xe.h"
#elif defined(STM32F410Tx)
#include "stm32f410tx.h"
#elif defined(STM32F410Cx)
#include "stm32f410cx.h"
#elif defined(STM32F410Rx)
#include "stm32f410rx.h"
#elif defined(STM32F411xE)
#include "stm32f411xe.h"
#elif defined(STM32F446xx)
#include "stm32f446xx.h"
#elif defined(STM32F469xx)
#include "stm32f469xx.h"
#elif defined(STM32F479xx)
#include "stm32f479xx.h"
#elif defined(STM32F412Cx)
#include "stm32f412cx.h"
#elif defined(STM32F412Zx)
#include "stm32f412zx.h"
#elif defined(STM32F412Rx)
#include "stm32f412rx.h"
#elif defined(STM32F412Vx)
#include "stm32f412vx.h"
#elif defined(STM32F413xx)
#include "stm32f413xx.h"
#elif defined(STM32F423xx)
#include "stm32f423xx.h"
#else
#error "Please select first the target STM32F4xx device used in your application (in stm32f4xx.h file)"
#endif

/**
  * @}
  */

/** @addtogroup Exported_types
  * @{
  */
typedef enum
{
  RESET = 0U,
  SET = !RESET
} FlagStatus, ITStatus;

typedef enum
{
  DISABLE = 0U,
  ENABLE = !DISABLE
} FunctionalState;
#define IS_FUNCTIONAL_STATE(STATE) (((STATE) == DISABLE) || ((STATE) == ENABLE))

typedef enum
{
  SUCCESS = 0U,
  ERROR = !SUCCESS
} ErrorStatus;

/**
  * @}
  */


/** @addtogroup Exported_macro
  * @{
  */
#define SET_BIT(REG, BIT)     ((REG) |= (BIT))

#define CLEAR_BIT(REG, BIT)   ((REG) &= ~(BIT))

#define READ_BIT(REG, BIT)    ((REG) & (BIT))

#define CLEAR_REG(REG)        ((REG) = (0x0))

#define WRITE_REG(REG, VAL)   ((REG) = (VAL))

#define READ_REG(REG)         ((REG))

#define MODIFY_REG(REG, CLEARMASK, SETMASK)  WRITE_REG((REG), (((READ_REG(REG)) & (~(CLEARMASK))) | (SETMASK)))

#define POSITION_VAL(VAL)     (__CLZ(__RBIT(VAL)))

/* Use of CMSIS compiler intrinsics for register exclusive access */
/* Atomic 32-bit register access macro to set one or several bits */
#define ATOMIC_SET_BIT(REG, BIT)                             \
  do {                                                       \
    uint32_t val;                                            \
    do {                                                     \
      val = __LDREXW((__IO uint32_t *)&(REG)) | (BIT);       \
    } while ((__STREXW(val,(__IO uint32_t *)&(REG))) != 0U); \
  } while(0)

/* Atomic 32-bit register access macro to clear one or several bits */
#define ATOMIC_CLEAR_BIT(REG, BIT)                           \
  do {                                                       \
    uint32_t val;                                            \
    do {                                                     \
      val = __LDREXW((__IO uint32_t *)&(REG)) & ~(BIT);      \
    } while ((__STREXW(val,(__IO uint32_t *)&(REG))) != 0U); \
  } while(0)

/* Atomic 32-bit register access macro to clear and set one or several bits */
#define ATOMIC_MODIFY_REG(REG, CLEARMSK, SETMASK)                          \
  do {                                                                     \
    uint32_t val;                                                          \
    do {                                                                   \
      val = (__LDREXW((__IO uint32_t *)&(REG)) & ~(CLEARMSK)) | (SETMASK); \
    } while ((__STREXW(val,(__IO uint32_t *)&(REG))) != 0U);               \
  } while(0)

/* Atomic 16-bit register access macro to set one or several bits */
#define ATOMIC_SETH_BIT(REG, BIT)                            \
  do {                                                       \
    uint16_t val;                                            \
    do {                                                     \
      val = __LDREXH((__IO uint16_t *)&(REG)) | (BIT);       \
    } while ((__STREXH(val,(__IO uint16_t *)&(REG))) != 0U); \
  } while(0)

/* Atomic 16-bit register access macro to clear one or several bits */
#define ATOMIC_CLEARH_BIT(REG, BIT)                          \
  do {                                                       \
    uint16_t val;                                            \
    do {                                                     \
      val = __LDREXH((__IO uint16_t *)&(REG)) & ~(BIT);      \
    } while ((__STREXH(val,(__IO uint16_t *)&(REG))) != 0U); \
  } while(0)

/* Atomic 16-bit register access macro to clear and set one or several bits */
#define ATOMIC_MODIFYH_REG(REG, CLEARMSK, SETMASK)                         \
  do {                                                                     \
    uint16_t val;                                                          \
    do {                                                                   \
      val = (__LDREXH((__IO uint16_t *)&(REG)) & ~(CLEARMSK)) | (SETMASK); \
    } while ((__STREXH(val,(__IO uint16_t *)&(REG))) != 0U);               \
  } while(0)

/**
  * @}
  */

#if defined (USE_HAL_DRIVER)
#include "stm32f4xx_hal.h"
#endif /* USE_HAL_DRIVER */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __STM32F4xx_H */
/**
  * @}
  */

/**
  * @}
  */
