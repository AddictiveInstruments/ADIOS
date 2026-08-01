// $Id: app.c 1920 2014-01-08 19:29:35Z tk $
/*
 * MIOS32 Application Template
 *
 * ==========================================================================
 *
 *  Copyright (C) <year> <your name> (<your email address>)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>
#include "app.h"
u32 count = 0;

/////////////////////////////////////////////////////////////////////////////
// This hook is called after startup to initialize the application
/////////////////////////////////////////////////////////////////////////////
void APP_Init(void)
{
	// initialize all LEDs
	MIOS32_BOARD_LED_Init(0xffffffff);

	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	LL_EXTI_InitTypeDef EXTI_InitStruct = {0};
	/* GPIO Ports Clock Enable */
	LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);

	GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	GPIO_InitStruct.Pin = LL_GPIO_PIN_8;
	LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

//	GPIO_InitStruct.Pin = LL_GPIO_PIN_2;
//	LL_GPIO_Init(GPIOB, &GPIO_InitStruct);
//	// EXTI TR5X6_DECOD_BUTT_COM
//	LL_EXTI_SetEXTISource(LL_EXTI_CONFIG_PORTB, LL_EXTI_CONFIG_LINE2);
//	EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_2;
//	EXTI_InitStruct.LineCommand = ENABLE;
//	EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
//	EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING;
//	LL_EXTI_Init(&EXTI_InitStruct);

	GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;

	GPIO_InitStruct.Pin = LL_GPIO_PIN_0;
	LL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	MIOS32_SYS_STM_PINSET(GPIOB, LL_GPIO_PIN_0, 1);
}


/////////////////////////////////////////////////////////////////////////////
// This task is running endless in background
/////////////////////////////////////////////////////////////////////////////
void APP_Background(void)
{

}


/////////////////////////////////////////////////////////////////////////////
// This hook is called each mS from the main task which also handles DIN, ENC
// and AIN events. You could add more jobs here, but they shouldn't consume
// more than 300 uS to ensure the responsiveness of buttons, encoders, pots.
// Alternatively you could create a dedicated task for application specific
// jobs as explained in $MIOS32_PATH/apps/tutorials/006_rtos_tasks
/////////////////////////////////////////////////////////////////////////////
void APP_Tick(void)
{
//	count++;
//	if(count<1000){
//		MIOS32_BOARD_LED_Set(1, 1);
//
//	}else{
//		MIOS32_BOARD_LED_Set(1, 0);
//		if(count>2000)count=0;
//	}
  // PWM modulate the status LED (this is a sign of life)
//  u32 timestamp = MIOS32_TIMESTAMP_Get();
// MIOS32_BOARD_LED_Set(1, (timestamp % 20) <= ((timestamp / 100) % 10));

	if(MIOS32_SYS_STM_PINGET(GPIOB, LL_GPIO_PIN_8)){
		MIOS32_BOARD_LED_Set(1, 0);
		MIOS32_SYS_STM_PINSET(GPIOB, LL_GPIO_PIN_0, 1);
	}else{
		MIOS32_BOARD_LED_Set(1, 1);
		MIOS32_SYS_STM_PINSET(GPIOB, LL_GPIO_PIN_0, 0);
	}
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called each mS from the MIDI task which checks for incoming
// MIDI events. You could add more MIDI related jobs here, but they shouldn't
// consume more than 300 uS to ensure the responsiveness of incoming MIDI.
/////////////////////////////////////////////////////////////////////////////
void APP_MIDI_Tick(void)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a MIDI package has been received
/////////////////////////////////////////////////////////////////////////////
void APP_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package)
{

//	if(midi_package.event==NoteOn){
//		 MIOS32_MIDI_SendDebugMessage("Note On\n");
//	  // forward USB0->UART0 and UART0->USB0
//	  switch( port ) {
//	    case USB0:
//	    	MIOS32_MIDI_SendPackage(USB0, midi_package);
//	    	MIOS32_MIDI_SendPackage(UART0,  midi_package);
//	    	MIOS32_MIDI_SendPackage(UART1,  midi_package);
//	    	break;
//	    case UART0:
//	    	MIOS32_MIDI_SendPackage(USB0, midi_package);
//	    	MIOS32_MIDI_SendPackage(UART0,  midi_package);
//	    	break;
//	  }
//	}
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called before the shift register chain is scanned
/////////////////////////////////////////////////////////////////////////////
void APP_SRIO_ServicePrepare(void)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called after the shift register chain has been scanned
/////////////////////////////////////////////////////////////////////////////
void APP_SRIO_ServiceFinish(void)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a button has been toggled
// pin_value is 1 when button released, and 0 when button pressed
/////////////////////////////////////////////////////////////////////////////
void APP_DIN_NotifyToggle(u32 pin, u32 pin_value)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when an encoder has been moved
// incrementer is positive when encoder has been turned clockwise, else
// it is negative
/////////////////////////////////////////////////////////////////////////////
void APP_ENC_NotifyChange(u32 encoder, s32 incrementer)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a pot has been moved
/////////////////////////////////////////////////////////////////////////////
void APP_AIN_NotifyChange(u32 pin, u32 pin_value)
{
}
