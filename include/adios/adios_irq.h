/*
 * This file collects all interrupt priorities and provides prototypes to
 * ADIOS_IRQ_* functions
 *
 * ==========================================================================
 *
 *  Copyright (C) 2026 Bruno Dupeyron (addictive.instruments@gmail.com)
 *  Licensed under MIT License.
 *  See the LICENSE file in the project root for full licence information.
 *
 * ==========================================================================
 */

#ifndef _ADIOS_IRQ_H
#define _ADIOS_IRQ_H


// we are using 4 bits for pre-emption priority, and no bits for subpriority
// this means: subpriority can always be set to 0, therefore no special 
// define is available for this setting
#define ADIOS_IRQ_PRIGROUP    (0x300)


// than lower the value, than higher the priority!

// note that FreeRTOS allows priority level < 10 for "SysCalls"
// means: FreeRTOS tasks can be interrupted by level<10 IRQs


// predefined user timer priorities (-> ADIOS_TIMER)
#define ADIOS_IRQ_PRIO_LOW       12  // lower than RTOS
#define ADIOS_IRQ_PRIO_MID        8  // higher than RTOS
#define ADIOS_IRQ_PRIO_HIGH       5  // same like SPI, AIN, etc...
#define ADIOS_IRQ_PRIO_HIGHEST    4  // higher than SPI, AIN, etc...



// DMA Channel IRQ used by ADIOS_SPI
#define ADIOS_IRQ_SPI_DMA_PRIORITY    ADIOS_IRQ_PRIO_HIGH


// DMA Channel IRQ used by ADIOS_I2S, 
// period depends on sample buffer size, but usually 1..2 mS
// relaxed conditions (since samples are transfered in background)
#define ADIOS_IRQ_I2S_DMA_PRIORITY     ADIOS_IRQ_PRIO_HIGH

// DMA Channel IRQ used by ADIOS_AIN, called after 
// all ADC channels have been converted
#define ADIOS_IRQ_ADC_DMA_PRIORITY     ADIOS_IRQ_PRIO_HIGH

// Shared DMA Channel IRQ used by LTC17xx port for all DMA channels!
#define ADIOS_IRQ_GLOBAL_DMA_PRIORITY    ADIOS_IRQ_PRIO_HIGH



// I2C IRQs used by ADIOS_I2C, called rarely on I2C accesses
// should be very high to overcome peripheral flaws
// estimated requirement for "reaction time": less than 9/400 kHz = 22.5 uS
// EV and ER IRQ share the same priority: they share resources, and on G0
// they are not even two vectors (I2C1_IRQn / I2C2_3_IRQn carry both)
#define ADIOS_IRQ_I2C_EV_PRIORITY      2
#define ADIOS_IRQ_I2C_ER_PRIORITY      2


// UART IRQs used by ADIOS_UART
// typically called each 320 mS if full MIDI bandwidth is used
// priority should be high to avoid data loss
#define ADIOS_IRQ_UART_PRIORITY        ADIOS_IRQ_PRIO_HIGHEST

// USB provides flow control - this interrupt can run at low priority (but higher than RTOS tasks)
// The interrupt is called at least each mS and takes ca. 1 uS to service the SOF (Start of Frame) flag

#define ADIOS_IRQ_USB_PRIORITY         ADIOS_IRQ_PRIO_MID



/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 ADIOS_IRQ_Disable(void);
extern s32 ADIOS_IRQ_Enable(void);

extern s32 ADIOS_IRQ_Install(u8 IRQn, u8 priority);
extern s32 ADIOS_IRQ_DeInstall(u8 IRQn);

#endif /* _ADIOS_IRQ_H */
