/****************************************************************************
 *                                                                          *
 *                                                                          *
 *                                                                          *
 *                                                                          *
 ****************************************************************************/
 
#include <adios.h> 

#include "max11300.h"
#include "max11300_defs.h"


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////
// default values
#define DEFAULT_THRESHOLD	0x03ff
#define TEMP_LSB			(0.125/16)
#define SOFT 0
#define HARD 1
// Compiler switches for the debug console
#define MAX11300_VERBOSE 2

#define MAX11300_INTERRUPT   void EXTI9_5_IRQHandler(void)

/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////
MAX_config_t MAX_Configs[MAX_NUM];
u8 MAX_Curr;
u16 polarity, zero_crossing;

/////////////////////////////////////////////////////////////////////////////
// Local prototypes
/////////////////////////////////////////////////////////////////////////////


//MAX11300::MAX11300(SPIClass *spi, uint8_t convertPin, uint8_t selectPin) {
//    _convertPin = convertPin;
//    _spi = spi;
//    _select = selectPin;
//    _interrupt = 255;
//    _analogStatus = 0;
//    pinMode(_convertPin, OUTPUT);
//    digitalWrite(_convertPin, HIGH);
//    _spiMode = new SPISettings(20000000L, LSBFIRST, SPI_MODE0);
//    _spi->begin();
//}
//
//MAX11300::MAX11300(SPIClass *spi, uint8_t convertPin, uint8_t selectPin, uint8_t interruptNumber) {
//    _convertPin = convertPin;
//    _spi = spi;
//    _select = selectPin;
//    _interrupt = interruptNumber;
//    _analogStatus = 0;
//    pinMode(_convertPin, OUTPUT);
//    pinMode(_select, OUTPUT);
//    digitalWrite(_convertPin, HIGH);
//    digitalWrite(_select, HIGH);
//    _spiMode = new SPISettings(20000000L, LSBFIRST, SPI_MODE0);
//    _spi->begin();
//    _spi->usingInterrupt(_interrupt);
//}eurorack 3d model


int8_t MAX11300_Init(u8 mode) {
    u8 p;
    u32 status;
    status=0;
    
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    /* Enable clock for GPIOB */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    /* Enable clock for SYSCFG */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    // Init MAX Config Array
    
#if MAX11300_VERBOSE >=1
    ADIOS_MIDI_SendDebugMessage("[MAX11300]%d PIXI(s) found.\n", MAX_NUM);
#endif
    for (MAX_Curr=0; MAX_Curr<MAX_NUM; MAX_Curr++) {

        /// dac prepare
        //MAX_Configs[MAX_Curr].status=-2; // no init
        MAX_Configs[MAX_Curr].CS_port=MAX_SPI_PERIPH_J16;
        MAX_Configs[MAX_Curr].CS_pin=MAX_SPI_PIN_RC2;
        MAX_Configs[MAX_Curr].status = 0;
        for (p=0; p<20; p++) {
            /// pin prepare
            MAX_Configs[MAX_Curr].ports[p].DACvalue=0;
        }
        // Init SPI.
        MAX_Configs[MAX_Curr].status = Hlp_SPI_Init(MAX_Curr);
        if ( MAX_Configs[MAX_Curr].status == 0x0424) {


            // Set Device Control Reg
            MAX_Configs[MAX_Curr].devCtrl.ALL = 0x01c0;
            writeRegister(MAX_DEVCTL, MAX_Configs[MAX_Curr].devCtrl.ALL );
            MAX_Configs[MAX_Curr].int_mask.ALL = 0xfffd;
            writeRegister(MAX_INTMASK, MAX_Configs[MAX_Curr].int_mask.ALL );
            MAX11300_ADCreadyPin(4);				// show that we've read the data from that pin
            MAX11300_ADCready();

            // Set Temp Config reg and read it.
            writeRegister(MAX_TMPMONCFG, 0x0000);
            MAX11300_readInternalTemp(HARD);
 #if MAX11300_VERBOSE >=1
            // Verbose Level 1 Status, internal Temp.
            ADIOS_MIDI_SendDebugMessage("[MAX11300]PIXI %d connected. Temp=%ddegC. \n", MAX_Curr+1, (MAX_Configs[MAX_Curr].TMPINTDATA >> 3));
#endif
        }else{
#if MAX11300_VERBOSE >=1
            ADIOS_MIDI_SendDebugMessage("[MAX11300]PIXI %d not connected. Status=%d \n", MAX_Curr+1, status);
#endif
        }
        
    }
    // Init CS lines.
        ADIOS_SPI_RC_PinSet(MAX_SPI_PERIPH, MAX_SPI_PIN_RC1, 1); // spi, rc_pin, pin_value
//    /* J16.RC1 (PB2) as Interrupt Input */
//    GPIO_InitStructure.GPIO_Pin = LL_GPIO_PIN_2;
//    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
//    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//    //GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
//    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
//    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    /***************************************************************/
    /* J10.14 (PE6) as Interrupt Input */
    /***************************************************************/
    GPIO_InitStructure.GPIO_Pin = LL_GPIO_PIN_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOE, &GPIO_InitStructure);
    /* Tell system that you will use PD0 for EXTI_Line0 */
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE, EXTI_PinSource6);
    /* PE6 is connected to EXTI_Line6 */
    EXTI_InitStructure.EXTI_Line = EXTI_Line6;
    /* Enable interrupt */
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    /* Interrupt mode */
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    /* Triggers on rising and falling edge */
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    /* Add to EXTI */
    EXTI_Init(&EXTI_InitStructure);
    /* Add IRQ vector to NVIC */
    /* PE6 is connected to EXTI_Line6, which has EXTI6_IRQn vector */
    NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
    /* Set priority */
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;
    /* Set sub priority */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;
    /* Enable interrupt */
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    /* Add to NVIC */
    NVIC_Init(&NVIC_InitStructure);
    //    // Tell system that you will use PB2 for EXTI_Line2
//    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource2);
//    // PB2 is connected to EXTI_Line2
//    EXTI_InitStructure.EXTI_Line = EXTI_Line2;
//    // Enable interrupt
//    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
//    // Interrupt mode
//    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
//    // Triggers on rising and falling edge */
//    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
//    // Add to EXTI
//    EXTI_Init(&EXTI_InitStructure);
//    // Add IRQ vector to NVIC */
//    // PB2 is connected to EXTI_Line2, which has EXTI6_IRQn vector */
//    NVIC_InitStructure.NVIC_IRQChannel = EXTI2_IRQn;
//    // Set priority
//    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;
//    // Set sub priority
//    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;
//    // Enable interrupt
//    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
//    // Add to NVIC
//    NVIC_Init(&NVIC_InitStructure);
//    
    
    // Verbose Level 2 Configuraton.
    // Verbose Level 1 Status, internal Temp.
    // Record Temp Flag.
    
    MAX_Curr=0;

    //MAX_Configs[MAX_Curr].int_mask.ALL = 0xffff;
    //writeRegister(MAX_INTMASK, MAX_Configs[MAX_Curr].int_mask.ALL );
//    MAX11300_setADCmode(ContinuousSweep);
//    MAX11300_setADCrate(rate200ksps);
//    MAX11300_setPinMode(4, ADC_Single);
//    MAX11300_setADCpinRef (4, ADCrefInt);
//    MAX11300_setADCpinAverage (4, average128);
//    MAX11300_setADCpinRange(4, Neg5to5);

    return 1;
}

int8_t MAX11300_Deinit(void) {
    //if (_interrupt < 255) detachInterrupt(_interrupt);
    return 1;
}

int8_t MAX11300_setPinMode(uint8_t pin, Func_t FuncID) {
    return MAX11300_setPinDiffMode(pin, FuncID, 255);
}

int8_t MAX11300_setPinDiffMode(uint8_t pin, Func_t FuncID, uint8_t differentialPin) {
    if(MAX_Curr>=MAX_NUM)return;
    uint16_t negConf;
    if (pin > 19) return 0;
    if (((FuncID == SwitchReg) || (FuncID == SwitchGPI) || (FuncID == ADC_DiffPos))
        && (differentialPin > 19)) return 0;	// if we don't have a valid additional pin, bail
    if (differentialPin == pin) return 0;
    if (((FuncID == SwitchReg) || (FuncID == SwitchGPI))
        && (((differentialPin - pin) >1) || ((differentialPin - pin) < -1) || differentialPin == pin) ) return 0;	// if we don't have a valid additional pin, bail
    // this configuration is invalid, so bail
    MAX_Configs[MAX_Curr].ports[pin].Config.FuncID = FuncID;
    //configuration = readRegister(MAX_FUNC_BASE + pin);							// read in the existing configuration so we don't stomp it
    switch (FuncID) {
        case HighZ:
            //Nothing to Do
            break;
        case GPI:
            if (MAX11300_getPinThreshold(pin) == 0) {
                MAX11300_setPinThreshold(pin, DEFAULT_THRESHOLD);
                ADIOS_DELAY_Wait_uS(1000);
            }
            break;
        case BILvlTrans:
            //ToDo
            break;
        case GPO:
            if (MAX11300_getPinThreshold(pin) == 0) {
                MAX11300_setPinThreshold(pin, DEFAULT_THRESHOLD);
                ADIOS_DELAY_Wait_uS(1000);
            }
            break;
        case UNIOutPath:
            //ToDo
            break;
        case ADC_Single:
            //ToCheck
            break;
        case DAC_Single:
            if(MAX_Configs[MAX_Curr].ports[pin].Config.Range == none)MAX_Configs[MAX_Curr].ports[pin].Config.Range = ZeroTo10;
            break;
        case ADC_DiffPos:
            negConf = readRegister(MAX_FUNC_BASE + differentialPin);
            negConf = (negConf & ~(MAX_FUNCID_MASK)) | MAX_FUNCID_ADC_DIFF_NEG;
            if (!writeRegister((MAX_FUNC_BASE + differentialPin), negConf)) return 0;
            break;
        case ADC_DiffNeg:
            //ToDo
            break;
        case ADC_DAC_Neg:
            //ToDo
            break;
        case SwitchGPI:
            //ToDo
            break;
        case SwitchReg:
            //ToDo
            break;
        default:
            return 0;
    }
    if (writeRegister((MAX_FUNC_BASE + pin), MAX_Configs[MAX_Curr].ports[pin].Config.ALL)) return 1;
    return 0;
}

Func_t MAX11300_getPinMode(uint8_t pin) {
    if(MAX_Curr>=MAX_NUM)return 0;
    return MAX_Configs[MAX_Curr].ports[pin].Config.FuncID;
}

int8_t MAX11300_getDifferentialPin(uint8_t pin) {
    uint16_t conf = readRegister(MAX_FUNC_BASE + pin);
    return (conf & MAX_FUNCPRM_ASSOC_MASK);
}

int8_t MAX11300_setPinThreshold(uint8_t pin, uint16_t voltage) {
return MAX11300_setDACpin(pin, voltage);
}

uint16_t MAX11300_getPinThreshold(uint8_t pin) {
    return readRegister(MAX_DACDAT_BASE + pin);
}

int8_t MAX11300_setDigitalInputMode(uint8_t pin, GPImode_t mode) {
    uint8_t gpiAddress, gpiOffset;
    uint16_t gpimdRegister;
    if (pin > 15) {
        gpiAddress = MAX_GPIMD_16_19;
        gpiOffset = 2*(pin - 16);
    } else if (pin > 7) {
        gpiAddress = MAX_GPIMD_8_15;
        gpiOffset = 2*(pin - 8);
    } else {
        gpiAddress = MAX_GPIMD_0_7;
        gpiOffset = 2*pin;
    }
    return readModifyWriteRegister(gpiAddress, (0x03 << gpiOffset), ((uint16_t)(mode) << gpiOffset));
}

GPImode_t MAX11300_getDigitalInputMode(uint8_t pin) {
    uint8_t gpiAddress, gpiOffset;
    uint16_t gpimdRegister;
    if (pin > 15) {
        gpiAddress = MAX_GPIMD_16_19;
        gpiOffset = 2*(pin - 16);
    } else if (pin > 7) {
        gpiAddress = MAX_GPIMD_8_15;
        gpiOffset = 2*(pin - 8);
    } else {
        gpiAddress = MAX_GPIMD_0_7;
        gpiOffset = 2*pin;
    }
    gpimdRegister = ((readRegister(gpiAddress) >> gpiOffset) && 0x03);
    if (gpimdRegister == 0x0) return GPIneither;
    if (gpimdRegister == 0x1) return GPIpositive;
    if (gpimdRegister == 0x2) return GPInegative;
    if (gpimdRegister == 0x3) return GPIboth;
    return GPINONE;
}


int8_t MAX11300_readDigitalPin (uint8_t pin) {
    uint8_t address, offset;
    uint16_t reg;
    if (pin > 15) {
        address = MAX_GPIDAT_H;
        offset = pin - 15;
    } else {
        address = MAX_GPIDAT_L;
        offset = pin;
    }
    reg = readRegister(address);
    return (int8_t)((reg >> offset) & 1);
}

int8_t MAX11300_writeDigitalPin (uint8_t pin, int8_t value) {
    uint8_t address, offset;
    uint16_t reg;
    if (pin > 15) {
        address = MAX_GPODAT_H;
        offset = pin - 15;
    } else {
        address = MAX_GPODAT_L;
        offset = pin;
    }
    reg = readRegister(address);
    if (value) {
        reg |= 1 << offset;
    } else {
        reg &= ~(1 << offset);
    }
    return writeRegister(address, reg);
    
}

/* ADC Data Ready Interrupt ******************************************************
 */
int8_t MAX11300_getInterrupt (void) {
    if(MAX_Curr>=MAX_NUM)return 0;
    MAX_Configs[MAX_Curr].interrupt.ALL = readRegister(MAX_INT);
    return MAX_Configs[MAX_Curr].interrupt.ALL;
}


void MAX11300_clearInterrupt (void){
    MAX_Configs[MAX_Curr].interrupt.ALL = 0x0000;
}
/******************************** ADC Routines ********************************/
/* ADC Conversion mode ****************************************************
Idle/SingleSweep/SingleSample/ContinuousSweep
*/
int8_t MAX11300_setADCmode (ADCmode_t mode) {
    if(MAX_Curr>=MAX_NUM)return 0;
    MAX_Configs[MAX_Curr].devCtrl.ADCCTL= mode;
    return writeRegister(MAX_DEVCTL, MAX_Configs[MAX_Curr].devCtrl.ALL );
}
ADCmode_t MAX11300_getADCmode (uint8_t fromHard) {
    if(MAX_Curr>=MAX_NUM)return 0;
    if(fromHard)MAX_Configs[MAX_Curr].devCtrl.ALL = readRegister(MAX_DEVCTL);
    return MAX_Configs[MAX_Curr].devCtrl.ADCCTL ;
}
/* ADC Conversion rate ****************************************************
rate200ksps/rate250ksps/rate333ksps/rate400ksps
 */
int8_t MAX11300_setADCrate(ADCRate_t rate) {
    if(MAX_Curr>=MAX_NUM)return 0;
    MAX_Configs[MAX_Curr].devCtrl.ADCCONV= rate;
    return writeRegister(MAX_DEVCTL, MAX_Configs[MAX_Curr].devCtrl.ALL );
}
ADCRate_t MAX11300_getADCrate(uint8_t fromHard) {
    if(MAX_Curr>=MAX_NUM)return 0;
    if(fromHard)MAX_Configs[MAX_Curr].devCtrl.ALL = readRegister(MAX_DEVCTL);
    return MAX_Configs[MAX_Curr].devCtrl.ADCCONV ;
}

/* ADC Ref ****************************************************************
 DACrefInt/ADC_DACref
*/
int8_t MAX11300_setADCpinRef (uint8_t pin, ADCref_t reference) {
    if(MAX_Curr>=MAX_NUM)return 0;
    MAX_Configs[MAX_Curr].ports[pin].Config.AVR= reference;
    return writeRegister(MAX_FUNC_BASE + pin, MAX_Configs[MAX_Curr].ports[pin].Config.ALL );
}
ADCref_t MAX11300_getADCpinRef (uint8_t pin, uint8_t fromHard) {
    if(MAX_Curr>=MAX_NUM)return 0;
    if(fromHard)MAX_Configs[MAX_Curr].ports[pin].Config.ALL = readRegister(MAX_FUNC_BASE + pin);
    return MAX_Configs[MAX_Curr].ports[pin].Config.AVR;
}
/* ADC Samples Averaging ****************************************************************
 1/2/4/8/16/32/64/128
 */
int8_t MAX11300_setADCpinAverage (uint8_t pin, int8_t samples) {
    if(MAX_Curr>=MAX_NUM)return 0;
    MAX_Configs[MAX_Curr].ports[pin].Config.Samples= samples;
    return writeRegister(MAX_FUNC_BASE + pin, MAX_Configs[MAX_Curr].ports[pin].Config.ALL );
}
int8_t MAX11300_getADCpinAverage (uint8_t pin, uint8_t fromHard) {
    if(MAX_Curr>=MAX_NUM)return 0;
    if(fromHard)MAX_Configs[MAX_Curr].ports[pin].Config.ALL = readRegister(MAX_FUNC_BASE + pin);
    return MAX_Configs[MAX_Curr].ports[pin].Config.Samples;
}
/* ADC Range ****************************************************************
none/ZeroTo10/Neg5to5/Neg10to0
 */
int8_t MAX11300_setADCpinRange (uint8_t pin, int8_t range) {
    if(MAX_Curr>=MAX_NUM)return 0;
    MAX_Configs[MAX_Curr].ports[pin].Config.Range= range;
    return writeRegister(MAX_FUNC_BASE + pin, MAX_Configs[MAX_Curr].ports[pin].Config.ALL );
}
int8_t MAX11300_getADCpinRange (uint8_t pin, uint8_t fromHard) {
    if(MAX_Curr>=MAX_NUM)return 0;
    if(fromHard)MAX_Configs[MAX_Curr].ports[pin].Config.ALL = readRegister(MAX_FUNC_BASE + pin);
    return MAX_Configs[MAX_Curr].ports[pin].Config.Range;
}
/* ADC Flag Interrupt ******************************************************
*/
int8_t MAX11300_ADCconvComplete (void) {
    if(MAX_Curr>=MAX_NUM)return 0;
    MAX_Configs[MAX_Curr].interrupt.ALL = readRegister(MAX_INT);
    return MAX_Configs[MAX_Curr].interrupt.ADCFLAG;
}
/* ADC Data Ready Interrupt ******************************************************
 */
int8_t MAX11300_ADCready (void) {
    if(MAX_Curr>=MAX_NUM)return 0;
    MAX_Configs[MAX_Curr].interrupt.ALL = readRegister(MAX_INT);
    return MAX_Configs[MAX_Curr].interrupt.ADCDR;
}
/* ADC Data Missed Interrupt ******************************************************
 */
int8_t MAX11300_ADCmissed (void) {
    if(MAX_Curr>=MAX_NUM)return 0;
    MAX_Configs[MAX_Curr].interrupt.ALL = readRegister(MAX_INT);
    return MAX_Configs[MAX_Curr].interrupt.ADCDM;
}
/* ADC Data Ready ******************************************************
 */
int8_t MAX11300_ADCreadyPin (uint8_t pin) {
    if(MAX_Curr>=MAX_NUM)return 0;
        // read in the analog status registers from the chip and update the internal tracking
        MAX_Configs[MAX_Curr].ADCSTATUS = (((uint32_t)(readRegister(MAX_ADCST_H))) << 16) |
        (readRegister(MAX_ADCST_L));
    if (MAX_Configs[MAX_Curr].ADCSTATUS & (1 << pin)) return 1;
    return 0;
    
}

/* ADC Data ******************************************************
 */
uint16_t MAX11300_getADCpin(uint8_t pin, uint8_t fromHard) {
    if(MAX_Curr>=MAX_NUM)return 0;
    if(!fromHard)return MAX_Configs[MAX_Curr].ports[pin].ADCvalue;
    switch(MAX11300_getADCmode(SOFT)) {
        case Idle:
            return 0;
            break;
        case SingleSweep:
        case SingleSample:
            while (!(MAX11300_ADCreadyPin(pin))) {
                startConversion();
                while (!(MAX11300_ADCconvComplete()));
            }
            break;
        case ContinuousSweep:
            break;
        default:
            return 0;
    }
    MAX_Configs[MAX_Curr].ports[pin].ADCvalue = readRegister(MAX_ADCDAT_BASE + pin);
    MAX11300_ADCreadyPin(pin);				// show that we've read the data from that pin
    MAX11300_ADCready();
    //ADIOS_MIDI_SendDebugMessage("[MAX11300]PIXI %d / %d TEST %x  %d \n", MAX_Curr, pin, readRegister(MAX_FUNC_BASE + pin) , MAX_Configs[MAX_Curr].ports[pin].ADCvalue );
    return MAX_Configs[MAX_Curr].ports[pin].ADCvalue;
}
    /******************************** DAC Routines ********************************/
/* DAC Mode ****************************************************************
 Sequential/Immediate/AllToPreset1/AllToPreset2
*/
int8_t MAX11300_setDACmode (DACmode_t mode) {
    if(MAX_Curr>=MAX_NUM)return;
    MAX_Configs[MAX_Curr].devCtrl.DACCTL= mode;
    return writeRegister(MAX_DEVCTL, MAX_Configs[MAX_Curr].devCtrl.ALL );
}
DACmode_t MAX11300_getDACmode (uint8_t fromHard) {
    if(MAX_Curr>=MAX_NUM)return;
    if(fromHard)MAX_Configs[MAX_Curr].devCtrl.ALL = readRegister(MAX_DEVCTL);
    return MAX_Configs[MAX_Curr].devCtrl.DACCTL ;
}
/* DAC Ref ****************************************************************
 DACrefExt/DACrefInt
*/
int8_t MAX11300_setDACref (DACref_t reference) {
    if(MAX_Curr>=MAX_NUM)return;
    MAX_Configs[MAX_Curr].devCtrl.DACREF= reference;
    return writeRegister(MAX_DEVCTL, MAX_Configs[MAX_Curr].devCtrl.ALL );
}
DACref_t MAX11300_getDACref (uint8_t fromHard) {
    if(MAX_Curr>=MAX_NUM)return;
    if(fromHard)MAX_Configs[MAX_Curr].devCtrl.ALL = readRegister(MAX_DEVCTL);
    return MAX_Configs[MAX_Curr].devCtrl.DACREF ;
}
/* DAC Range ****************************************************************
none/ZeroTo10/Neg5to5/Neg10to0
*/
int8_t MAX11300_setDACpinRange (uint8_t pin, Range_t range) {
    if(MAX_Curr>=MAX_NUM)return;
    MAX_Configs[MAX_Curr].ports[pin].Config.Range = range;
    return writeRegister(MAX_FUNC_BASE + pin, MAX_Configs[MAX_Curr].ports[pin].Config.ALL );
}
Range_t MAX11300_getDACpinRange (uint8_t pin, uint8_t fromHard) {
    if(MAX_Curr>=MAX_NUM)return;
    if(fromHard)MAX_Configs[MAX_Curr].ports[pin].Config.ALL = readRegister(MAX_FUNC_BASE + pin);
    return MAX_Configs[MAX_Curr].ports[pin].Config.Range;
}
/* DAC Value ****************************************************************
 0 - 0x0fff
 */
int8_t MAX11300_setDACpin (uint8_t pin, uint16_t value) {
    if(MAX_Curr>=MAX_NUM)return 0;
    MAX_Configs[MAX_Curr].ports[pin].DACvalue = value;
    return writeRegister((MAX_DACDAT_BASE + pin), MAX_Configs[MAX_Curr].ports[pin].DACvalue);
}
DACref_t MAX11300_getDACpin (uint8_t pin, uint8_t fromHard) {
    if(MAX_Curr>=MAX_NUM)return;
    if(fromHard)MAX_Configs[MAX_Curr].ports[pin].DACvalue = readRegister(MAX_DACDAT_BASE + pin);
    return MAX_Configs[MAX_Curr].ports[pin].DACvalue ;
}

/******************************** TEMP Routines ********************************/
// Internal ****************************************************************
uint8_t MAX11300_readInternalTemp (uint8_t fromHard) {
    if(MAX_Curr>=MAX_NUM)return;
    if(fromHard)MAX_Configs[MAX_Curr].TMPINTDATA = readRegister(MAX_TMPINTDAT);
    return MAX_Configs[MAX_Curr].TMPINTDATA;
}
// External 1 **************************************************************
double MAX11300_readExternalTemp1 (uint8_t fromHard) {
    if(MAX_Curr>=MAX_NUM)return;
    if(fromHard)MAX_Configs[MAX_Curr].TMPEXT1DATA = readRegister(MAX_TMPEXT1DAT);
    return MAX_Configs[MAX_Curr].TMPEXT1DATA;
}
// External 2 **************************************************************
double MAX11300_readExternalTemp2 (uint8_t fromHard) {
    if(MAX_Curr>=MAX_NUM)return;
    if(fromHard)MAX_Configs[MAX_Curr].TMPEXT2DATA = readRegister(MAX_TMPEXT2DAT);
    return MAX_Configs[MAX_Curr].TMPEXT2DATA;
}


int8_t burstAnalogRead (uint16_t* samples, uint8_t size) {
    return burstAnalogReadPin(0, samples, size);
}
int8_t burstAnalogReadPin (uint8_t startPin, uint16_t* samples, uint8_t size) {
    if (size > 20) size = 20;
    return readRegisterS((MAX_ADCDAT_BASE + startPin), samples, size);
}

int8_t burstAnalogWrite (uint16_t* samples, uint8_t size) {
    return burstAnalogWritePin(0, samples, size);
}
int8_t burstAnalogWritePin (uint8_t startPin, uint16_t* samples, uint8_t size) {
    if (size > 20) size = 20;
    return writeRegisterS((MAX_DACDAT_BASE + startPin), samples, size);
}




int8_t writeRegister (uint8_t address, uint16_t value) {
    return writeRegisterS(address, &value, 1);
}

int8_t writeRegisterS (uint8_t address, uint16_t * values, uint8_t size) {
    u8 i;
    if(!MAX_Configs[MAX_Curr].status)return 0;
    //ADIOS_SPI_TransferModeInit(PIXI_SPI_PERIPH, ADIOS_SPI_MODE_CLK0_PHASE0, ADIOS_SPI_PRESCALER_128);
    ADIOS_SPI_TransferModeInit(MAX_SPI_PERIPH, ADIOS_SPI_MODE_CLK0_PHASE0, ADIOS_SPI_PRESCALER_128);
    ADIOS_SPI_RC_PinSet(MAX_SPI_PERIPH, MAX_Configs[MAX_Curr].CS_pin, 0); // spi, rc_pin, pin_value
    ADIOS_SPI_TransferByte(MAX_SPI_PERIPH, (address << 1));
    for (i = 0; i < size; i++) {
        ADIOS_SPI_TransferByte(MAX_SPI_PERIPH, ((values[i] >> 8) & 0xff));
        ADIOS_SPI_TransferByte(MAX_SPI_PERIPH, (values[i] & 0xff));
    }
    ADIOS_SPI_RC_PinSet(MAX_SPI_PERIPH, MAX_Configs[MAX_Curr].CS_pin, 1); // spi, rc_pin, pin_value
    return 1;
}

uint16_t readRegister (uint8_t address) {
    uint16_t val = 0;
    readRegisterS(address, &val, 1);
    return val;
}

uint16_t readRegisterS (uint8_t address, uint16_t * values, uint8_t size) {
    u8 i;
    if(!MAX_Configs[MAX_Curr].status)return 0;
    //ADIOS_SPI_TransferModeInit(PIXI_SPI_PERIPH, ADIOS_SPI_MODE_CLK0_PHASE0, ADIOS_SPI_PRESCALER_128);
    ADIOS_SPI_TransferModeInit(MAX_SPI_PERIPH, ADIOS_SPI_MODE_CLK0_PHASE0, ADIOS_SPI_PRESCALER_128);
    ADIOS_SPI_RC_PinSet(MAX_SPI_PERIPH, MAX_Configs[MAX_Curr].CS_pin, 0); // spi, rc_pin, pin_value
    ADIOS_SPI_TransferByte(MAX_SPI_PERIPH, ((address << 1) | 1));
    for (i = 0; i < size; i++) {
        values[i] = ((uint16_t)(ADIOS_SPI_TransferByte(MAX_SPI_PERIPH, 0x00)) << 8);
        values[i] += ((uint16_t)(ADIOS_SPI_TransferByte(MAX_SPI_PERIPH, 0x00)));
    }
    ADIOS_SPI_RC_PinSet(MAX_SPI_PERIPH, MAX_Configs[MAX_Curr].CS_pin, 1); // spi, rc_pin, pin_value
    return size;
}

void startConversion(void) {
//    digitalWrite(_convertPin, LOW);
//    delayMicroseconds(1);
//    digitalWrite(_convertPin, HIGH);
}

int8_t readModifyWriteRegister(uint8_t address, uint16_t mask, uint16_t value) {
    uint16_t reg = readRegister(address);
    reg = (reg & ~mask) | (uint16_t)(value);
    return writeRegister(address, reg);
}

double convertTemp_Int16toDbl (uint16_t temp) {
    return ((int16_t)(temp << 4) * TEMP_LSB);
}

uint16_t convertTemp_DbltoInt16 (double temp) {
    return (uint16_t)(((int16_t)(temp/TEMP_LSB)) >> 4);
}


/////////////////////////////////////////////////////////////////////////////
s32 Hlp_SPI_Init(u8 m)
{
    // check if valid MAX
    if(MAX_Curr>=MAX_NUM)return 0;
    u8 reg[3];
    s32 status = 0;
    // init SPI for MAX11300
//    if(MAX_Configs[MAX_Curr]==MAX_SPI_PINS_OD){
//            // ports in open drain mode (to pull-up the outputs to 5V)
//            status |= ADIOS_SPI_IO_Init(PIXI_SPI_PERIPH, ADIOS_SPI_PIN_DRIVER_STRONG_OD);
//        }else{
//            // ports in push-poll mode (3.3V output voltage)
            status |= ADIOS_SPI_IO_Init(MAX_SPI_PERIPH, ADIOS_SPI_PIN_DRIVER_STRONG);
//        }
//    }

    // init SPI
    status |= ADIOS_SPI_TransferModeInit(MAX_SPI_PERIPH, ADIOS_SPI_MODE_CLK0_PHASE0, ADIOS_SPI_PRESCALER_128);
    
    // read DEVICE_ID
    //Temporary access SPI Read
    MAX_Configs[MAX_Curr].status=1;
    status = readRegister (MAX_DEVICE_ID);
    MAX_Configs[MAX_Curr].status=0;
    
    return status; // DAC init error?
}


//MAX11300Event_MAX11300Event(void) {
//    this->clearEvent();
//}
//
//void MAX11300Event_clearEvent(void) {
//    lastIntVector = 0;
//    event = eventNONE;
//    status = 0;
//}


MAX11300_INTERRUPT
{
    if( EXTI_GetITStatus(EXTI_Line6) != RESET ) {
        if(!GPIO_ReadInputDataBit(GPIOE, LL_GPIO_PIN_6)){
            if(MAX11300_getInterrupt ()){

                //if(MAX_Configs[MAX_Curr].interrupt.ADCFLAG)ADIOS_MIDI_SendDebugMessage("1\n");
                if(MAX_Configs[MAX_Curr].interrupt.ADCDR &&  MAX11300_ADCreadyPin (4) ){
                    MAX_Configs[MAX_Curr].interrupt.ADCDR=0;
                    u16 polarity = MAX_Configs[MAX_Curr].ports[4].ADCvalue ;
                    MAX_Configs[MAX_Curr].ports[4].ADCvalue = readRegister(MAX_ADCDAT_BASE + 4);
                    //if(MAX_Configs[MAX_Curr].ports[4].ADCvalue>=(oldVal+2) || MAX_Configs[MAX_Curr].ports[4].ADCvalue<=(oldVal-2)){
                        
                      //  ADIOS_MIDI_SendDebugMessage("%d\n", MAX_Configs[MAX_Curr].ports[4].ADCvalue);
                    //}

                }
                if(MAX_Configs[MAX_Curr].interrupt.ADCDM)ADIOS_MIDI_SendDebugMessage("3\n");
                if(MAX_Configs[MAX_Curr].interrupt.GPIDR)ADIOS_MIDI_SendDebugMessage("4\n");
                if(MAX_Configs[MAX_Curr].interrupt.GPIDM)ADIOS_MIDI_SendDebugMessage("5\n");
                if(MAX_Configs[MAX_Curr].interrupt.DACOI)ADIOS_MIDI_SendDebugMessage("6\n");
                //if(MAX_Configs[MAX_Curr].interrupt.TMPINT)ADIOS_MIDI_SendDebugMessage("7\n");
                if(MAX_Configs[MAX_Curr].interrupt.TMPEXT1)ADIOS_MIDI_SendDebugMessage("8\n");
                if(MAX_Configs[MAX_Curr].interrupt.TMPEXT2)ADIOS_MIDI_SendDebugMessage("9\n");
                if(MAX_Configs[MAX_Curr].interrupt.VMON)ADIOS_MIDI_SendDebugMessage("a\n");
                MAX11300_clearInterrupt();
            }
        

			// show that we've read the data from that pin

			// show that we've read the data from that pin
        /* Clear interrupt flag */
        }
        EXTI_ClearITPendingBit(LL_GPIO_PIN_6);
            
    }
}

    //    lastEvent.time = micros();							// set the time as soon as possible
    //    uint16_t lastIntVector = lastEvent.lastIntVector;	// copy the last interrupt vector over for comparison
    //    lastEvent.clearEvent();								// clears everything except the time
    //    lastEvent.lastIntVector = readRegister(MAX_INT);	// load the latest interrupt vector
    //    uint16_t delta = ((lastEvent.lastIntVector ^ lastIntVector) &
    //                      lastEvent.lastIntVector);
    //    if (delta & MAX_VMON_MASK) {
    //        lastEvent.event = VoltageMonitor;
    //    } else if (delta & MAX_TMPINT_MASK) {
    //        if (delta & (1 << MAX_TMPINT_AVAIL)) lastEvent.event = InternalTempAvailable;
    //        if (delta & (1 << MAX_TMPINT_HI)) lastEvent.event = InternalTempMonitorHigh;
    //        if (delta & (1 << MAX_TMPINT_LO)) lastEvent.event = InternalTempMonitorLow;
    //    } else if (delta & MAX_TMPEXT1_MASK) {
    //        if (delta & (1 << MAX_TMPEXT1_AVAIL)) lastEvent.event = ExternalTemp1Available;
    //        if (delta & (1 << MAX_TMPEXT1_HI)) lastEvent.event = ExternalTemp1MonitorHigh;
    //        if (delta & (1 << MAX_TMPEXT1_LO)) lastEvent.event = ExternalTemp1MonitorLow;
    //    } else if (delta & MAX_TMPEXT2_MASK) {
    //        if (delta & (1 << MAX_TMPEXT2_AVAIL)) lastEvent.event = ExternalTemp2Available;
    //        if (delta & (1 << MAX_TMPEXT2_HI)) lastEvent.event = ExternalTemp2MonitorHigh;
    //        if (delta & (1 << MAX_TMPEXT2_LO)) lastEvent.event = ExternalTemp2MonitorLow;
    //    } else if (delta & MAX_DACOI_MASK) {
    //        lastEvent.event = DACovercurrent;
    //        lastEvent.status = (((uint32_t)(readRegister(MAX_DACOI_H))) << 16) |
    //        (readRegister(MAX_DACOI_L));
    //    } else if (delta & MAX_GPIDM_MASK) {
    //        lastEvent.event = DigitalDataMissed;
    //        lastEvent.status = (((uint32_t)(readRegister(MAX_GPIST_H))) << 16) |
    //        (readRegister(MAX_GPIST_L));
    //    } else if (delta & MAX_GPIDR_MASK) {
    //        lastEvent.event = DigitalDataReady;
    //        lastEvent.status = (((uint32_t)(readRegister(MAX_GPIST_H))) << 16) |
    //        (readRegister(MAX_GPIST_L));
    //    } else if (delta & MAX_ADCDM_MASK) {
    //        lastEvent.event = AnalogDataMissed;
    //    } else if (delta & MAX_ADCDR_MASK) {
    //        lastEvent.event = AnalogDataReady;
    //        isAnalogDataReady(0);
    //        lastEvent.status = _analogStatus;
    //    } else if (delta & MAX_ADCFLAG_MASK) {
    //        lastEvent.event = AnalogConversionComplete;
    //        _analogFlag = 1;
    //    } else {
    //        lastEvent.event = eventNONE;
    //    }
//}

//
//MAX11300Event getLastEvent (void) {
//    return lastEvent;
//}
