#ifndef __MAX11300_H__
#define __MAX11300_H__

#include "max11300_defs.h"

//Port Mode
typedef enum {
    HighZ =             0x0,
    GPI =               0x1,
    BILvlTrans =        0x2,
    GPO =               0x3,
    UNIOutPath =        0x4,
    DAC_Single = 		0x5,
    DAC_wMon = 			0x6,
    ADC_Single =        0x7,
    ADC_DiffPos =       0x8,
    ADC_DiffNeg =       0x9,
    ADC_DAC_Neg =       0xa,
    SwitchGPI =         0xb,
    SwitchReg =         0xc
}Func_t;

// DAC config
typedef enum {
    Sequential      = 	0,
    Immediate       = 	1,
    AllToPreset1    =	2,
    AllToPreset2	=	3
}DACmode_t;
typedef enum {
    DACrefExt = 	0,
    DACrefInt = 	1
}DACref_t;
typedef enum {
    none = 		0,
    ZeroTo10 = 			1,
    Neg5to5 =           2,
    Neg10to0 =          3
}Range_t;


// ADC config
typedef enum {
    ADCrefInt       =	0,
    ADC_DACref      = 	1
}ADCref_t;
typedef enum {
    Idle = 				0,
    SingleSweep     = 	1,
    SingleSample    =	2,
    ContinuousSweep	=	3
}ADCmode_t;

typedef enum {
    rate200ksps	= 		0,
    rate250ksps = 		1,
    rate333ksps = 		2,
    rate400ksps = 		3,
}ADCRate_t;
typedef enum {
    average1    = 	0,
    average2    = 	1,
    average4    =	2,
    average8	=	3,
    average16   = 	4,
    average32   = 	5,
    average64   =	6,
    average128	=	7
}ADCsamples_t;

//GPI config
typedef enum {
    GPIneither	= 		0x0,
    GPIpositive	= 		0x1,
    GPInegative	= 		0x2,
    GPIboth		= 		0x3,
    GPINONE = 			0xffff
}GPImode_t;





typedef enum {
    VoltageMonitor,
    InternalTempMonitorHigh,
    ExternalTemp1MonitorHigh,
    ExternalTemp2MonitorHigh,
    InternalTempMonitorLow,
    ExternalTemp1MonitorLow,
    ExternalTemp2MonitorLow,
    InternalTempAvailable,
    ExternalTemp1Available,
    ExternalTemp2Available,
    DACovercurrent,
    DigitalDataReady,
    DigitalDataMissed,
    AnalogDataReady,
    AnalogDataMissed,
    AnalogConversionComplete,
    eventNONE
}eventType_t;

typedef   struct {
    u8 bit:1;
} DATABit_t;
typedef union {
    u32 ALL;
    struct {
        DATABit_t bits[20];
        u32       unused:12;
    };
} DATAReg_t;

// Port ****************************************************************
// Port Configuration 0-19 Registers Bit Fields
typedef union {
    u16 ALL;
    struct {
        u16         AssocPort:5;
        u16         Samples:3;
        Range_t     Range:3;
        ADCref_t    AVR:1;
        Func_t      FuncID:4;
    };
} PortCfg_t;

// Port/Pin Data Structure
typedef struct {
    PortCfg_t    Config;
    u16          DACvalue;
    u16          ADCvalue;
    //s32 (*receive_callback_func)(u16 ADCvalue);
    DATABit_t   *GPIvalue;
    DATABit_t   *GPOvalue;

} PinPort_t;
// Common ********************************************************************
// Device Control Register Bit Fields
typedef union {
    u16 ALL;
    struct {
        ADCmode_t       ADCCTL:2;
        u16             DACCTL:2;
        ADCRate_t       ADCCONV:2;
        DACref_t        DACREF:1;
        u16             THSHDN:1;
        u16             TmPCTL:3;
        u16             TMPPER:1;
        u16             RS_CANCEL:1;
        u16             LPEN:1;
        u16             BRST:1;
        u16             Reset:1;
    };
} DevCtrl_t;
// Interrupt and Int Mask Register Bit Fields
typedef union {
    u16 ALL;
    struct {
        u16 ADCFLAG:1;
        u16 ADCDR:1;
        u16 ADCDM:1;
        u16 GPIDR:1;
        u16 GPIDM:1;
        u16 DACOI:1;
        u16 TMPINT:3;
        u16 TMPEXT1:3;
        u16 TMPEXT2:3;
        u16 VMON:1;
    };
} Interrupt_t;
// MAX11300 Data Structure
typedef struct MAX11300{
    u16         status;
    u8          CS_port;
    u8          CS_pin;
    Interrupt_t interrupt;
    PinPort_t   ports[20];
    u32         ADCSTATUS;
    u32         DACOISTATUS;
    u16         TMPINTDATA;
    u16         TMPEXT1DATA;
    u16         TMPEXT2DATA;
    DATAReg_t   GPIDATA;
    u32         GPODATA;
    DevCtrl_t   devCtrl;
    Interrupt_t int_mask;
    u16         DACPRSTDAT1;
    u16         DACPRSTDAT2;
} MAX_config_t;



//class MAX11300Event {
//
//    // Methods
//    MAX11300Event(void);
//    void clearEvent(void);
//    
//    // Members
//    unsigned long 	time;			// the time in microseconds at which the last event occurred
//    uint16_t 		lastIntVector;	// the interrupt vector the last time this was written
//    eventType_t 	event;			// type of interrupt event
//    uint32_t 		status;			// shows which pin(s) are responsible
//
//};

//typedef struct  {
//    // Methods
//    void OnEvent(void);
//    void clearEvent(void);
//    
//    // Members
//    unsigned long 	time;			// the time in microseconds at which the last event occurred
//    uint16_t 		lastIntVector;	// the interrupt vector the last time this was written
//    eventType_t 	event;			// type of interrupt event
//    uint32_t 		status;			// shows which pin(s) are responsible
//}MAX11300Event_t;

    // Methods
    /**
     * Constructor
     *
     * @param spi - The SPI interface the MAX11300 is attached to.
     * @param convertPin - The pin attached to the CNVT pin on the MAX11300
     *
     */
//    MAX11300(SPIClass* spi, uint8_t convertPin, uint8_t selectPin);
//    MAX11300(SPIClass* spi, uint8_t convertPin, uint8_t selectPin, uint8_t interruptNumber);
//    
    extern int8_t MAX11300_Init(u8 mode) ;
    extern int8_t MAX11300_Deinit(void);
    
    /**
     * Set the given pin to the given mode
     *
     * @param pin - The target MAX11300 pin
     * @param mode - The mode of the target MAX11300 pin
     * @param differentialPin - If the mode selected is analogDifferential, then add the second pin to use.
     * @retval true - The pin mode setting succeeded
     * @retval false - The pin mode setting failed, or no differential pin was given for analogDifferential
     */
    extern int8_t MAX11300_setPinMode(uint8_t pin, Func_t FuncID);
    extern int8_t MAX11300_setPinDiffMode(uint8_t pin, Func_t FuncID, uint8_t differentialPin);
    
    /**
     * Read the mode of the given pin
     *
     * @param pin - The pin to read from
     */
    extern Func_t MAX11300_getPinMode(uint8_t pin);
    
    /**
     * Read the differential partner of a given pin
     *
     * @param pin - The pin to read the differential partner of
     * @retval The differential partner of pin, or -1 if the given pin is not in differential mode
     */
    extern int8_t MAX11300_getDifferentialPin(uint8_t pin);
    
    /**
     * Set the given pin's digital logic threshold or digital output voltage
     *
     * @param pin - The target MAX11300 pin
     * @param voltage - The threshhold or output voltage
     */
    extern int8_t MAX11300_setPinThreshold(uint8_t pin, uint16_t voltage);
    
    /**
     * Get the given pin's digital logic threshold or digital output voltage
     *
     * @param pin - The target MAX11300 pin
     * @retval false - The threshold or output value
     */
    extern uint16_t     MAX11300_getPinThreshold(uint8_t pin);
    extern int8_t       MAX11300_setDigitalInputMode(uint8_t pin, GPImode_t mode);
    extern GPImode_t    MAX11300_getDigitalInputMode(uint8_t pin);
    
    extern int8_t       MAX11300_readDigitalPin (uint8_t pin);
    extern int8_t       MAX11300_writeDigitalPin (uint8_t pin, int8_t value);

    extern int8_t       MAX11300_getInterrupt (void);
    extern void       MAX11300_clearInterrupt (void);
/******************************** ADC Routines ********************************/
    extern int8_t       MAX11300_setADCmode (ADCmode_t mode);
    extern ADCmode_t    MAX11300_getADCmode (uint8_t fromHard);
    extern int8_t       MAX11300_setADCrate(ADCRate_t rate);
    extern ADCRate_t    MAX11300_getADCrate(uint8_t fromHard);
    extern int8_t       MAX11300_setADCpinRef (uint8_t pin, ADCref_t reference);
    extern ADCref_t     MAX11300_getADCpinRef (uint8_t pin, uint8_t fromHard);
    extern int8_t       MAX11300_setADCpinAverage (uint8_t pin, int8_t samples);
    extern int8_t       MAX11300_getADCpinAverage (uint8_t pin, uint8_t fromHard);
    extern int8_t       MAX11300_setADCpinRange (uint8_t pin, int8_t range);
    extern int8_t       MAX11300_getADCpinRange (uint8_t pin, uint8_t fromHard);
    extern int8_t       MAX11300_ADCconvComplete (void);
    extern int8_t       MAX11300_ADCready (void);
    extern int8_t       MAX11300_ADCmissed (void);
    extern int8_t       MAX11300_ADCreadyPin (uint8_t pin);
    extern uint16_t     MAX11300_getADCpin(uint8_t pin, uint8_t fromHard);
/******************************** DAC Routines ********************************/
    extern int8_t       MAX11300_setDACmode (DACmode_t mode);
    extern DACmode_t    MAX11300_getDACmode (uint8_t fromHard);
    extern int8_t       MAX11300_setDACref (DACref_t reference);
    extern DACref_t     MAX11300_getDACref (uint8_t fromHard);
    extern int8_t       MAX11300_setDACpinRange (uint8_t pin, Range_t range);
    extern Range_t   MAX11300_getDACpinRange (uint8_t pin, uint8_t fromHard);
    extern int8_t       MAX11300_setDACpin (uint8_t pin, uint16_t value);
    extern DACref_t     MAX11300_getDACpin (uint8_t pin, uint8_t fromHard);
/******************************** TEMP Routines ********************************/
    extern uint8_t      MAX11300_readInternalTemp (uint8_t fromHard);
    extern double       MAX11300_readExternalTemp1 (uint8_t fromHard);
    extern double       MAX11300_readExternalTemp2 (uint8_t fromHard);


    int8_t burstAnalogRead (uint16_t* samples, uint8_t size);
    int8_t burstAnalogWrite (uint16_t* samples, uint8_t size);
    int8_t burstAnalogReadPin (uint8_t startPin, uint16_t* samples, uint8_t size);
    int8_t burstAnalogWritePin (uint8_t startPin, uint16_t* samples, uint8_t size);

//    void serviceInterrupt(void);
    //MAX11300Event getLastEvent (void);
    
   
    // Methods
    int8_t writeRegister (uint8_t address, uint16_t value);
    int8_t writeRegisterS (uint8_t address, uint16_t* values, uint8_t size);
    uint16_t readRegister (uint8_t address);
    uint16_t readRegisterS (uint8_t address, uint16_t* values, uint8_t size);
    int8_t readModifyWriteRegister (uint8_t address, uint16_t mask, uint16_t value);
    void startConversion(void);
    double convertTemp_Int16toDbl (uint16_t temp);
    uint16_t convertTemp_DbltoInt16 (double temp);
    s32 Hlp_SPI_Init(u8 m);
    // Members
//    SPIClass* 	_spi; 
//    SPISettings* _spiMode;
    uint8_t 	_convertPin;
    uint8_t 	_interrupt;
    uint8_t 	_select;
    uint32_t	_analogStatus;
    int8_t		_analogFlag;
    //MAX11300Event lastEvent;
    
    //extern MAX_config_t *MAX_Configs;

#endif
