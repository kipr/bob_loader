#include "sensors.h"

#include <bob.h>
#include <pio/pio.h>
#include <adc/adc.h>
#include <pmc/pmc.h>
#include <timer/timer.h>


// Pin Setup Arrays
const Pin analogs[] = {ANALOG_SENSORS};
const Pin analogPullup[] = {PULLUP_EN,PULLUP_DATA,PULLUP_ADDA,PULLUP_ADDB,PULLUP_ADDC};
const Pin digitalIns[] = {DIGITAL_INPUTS};
const Pin digitalOuts[] = {DIGITAL_OUTPUTS};
const Pin sensorsPower[] = {SENSOR_POWER,BATTV_SELECT,BLACK_BUTTON};
const Pin motorFeedback[] = {MADC};

volatile int g_AnalogReading[16];
volatile int g_BatteryCounter = 0;
volatile int g_BatteryVoltage = 0;
static void ISR_Pit(void);

void SensorsInit()
{
	int i;
	PIO_Configure(analogs, PIO_LISTSIZE(analogs));
	PIO_Configure(analogPullup, PIO_LISTSIZE(analogPullup));
	PIO_Configure(digitalIns, PIO_LISTSIZE(digitalIns));
	PIO_Configure(sensorsPower, PIO_LISTSIZE(sensorsPower));
	PIO_Configure(motorFeedback, PIO_LISTSIZE(motorFeedback));
	
	PMC_EnablePeripheral(AT91C_ID_PIOB);
	PMC_EnablePeripheral(AT91C_ID_PIOA);
	
	PMC_EnablePeripheral(AT91C_ID_ADC0);
	ADC_Initialize(AT91C_BASE_ADC0, AT91C_ID_ADC0, AT91C_ADC_TRGEN_DIS, \
				   0, AT91C_ADC_SLEEP_NORMAL_MODE, AT91C_ADC_LOWRES_10_BIT, \
				   MCK, BOARD_ADC_FREQ, 10, 600);
	for(i = 0;i < 8;i++) ADC_EnableChannel(AT91C_BASE_ADC0, i);
	
	PMC_EnablePeripheral(AT91C_ID_ADC1);
	ADC_Initialize(AT91C_BASE_ADC1, AT91C_ID_ADC1, AT91C_ADC_TRGEN_DIS, \
				   0, AT91C_ADC_SLEEP_NORMAL_MODE, AT91C_ADC_LOWRES_10_BIT, \
				   MCK, BOARD_ADC_FREQ, 10, 600);
	for(i = 0;i < 8;i++) ADC_EnableChannel(AT91C_BASE_ADC1, i);

	InitPit(MC_PID_PERIOD,ISR_Pit);
	
	SelectAng0();		// select analog port 0 to be mesured instead of BattVoltage
	SensorPowerOn();	// turn on the Vcc line
	for(i = 0;i < 8;i++) AnalogPullUp(i,0);
}

static void ISR_Pit(void)
{
	unsigned int time;
	
	time = GetPITValueReset();
	PITInterruptLength(1);
}

int BlackButton()
{
  return !PIO_Get(&(sensorsPower[3]));
}

int Digital(int port)
{
	if(port < 0 || port > 8)
	  return 0;
	
	return !PIO_Get(&(digitalIns[port]));
}

int AllDigitals()
{
  int i = 0;
  int value = 0;
  
  for(i = 0;i < 8;i++) value |= Digital(i)<<i;
  
  return value;
}

// state = 1 pin is HIGH
// state = 0 pin is LOW
void DigitalOut(int port, int state)
{	
	if(port < 0 || port > 8) 
		return;
	
	if(state)
		PIO_Set(&(digitalOuts[port]));
	else
		PIO_Clear(&(digitalOuts[port]));
}

// dir = 0 is input
// dir = 1 is output
void DigitalConfig(int port, int dir)
{
	if(port < 0 || port > 8) 
		return;
	
	if(dir)
		PIO_Configure(&(digitalOuts[port]),1);
	else
		PIO_Configure(&(digitalIns[port]),1);
}

// returns 1 if the port is an output
// returns 0 if the port is an input
// returns -1 if port is out of range
int GetDigitalConfig(int port)
{
	if(port < 0 || port > 8) 
		return -1;
	
	if(PIO_IsOutputPin(&(digitalIns[port]))) return 1;
	else return 0;
}

void UpdateAnalogs(void)
{
	ADC_StartConversion(AT91C_BASE_ADC0);
	while((ADC_GetStatus(AT91C_BASE_ADC0) & 0x80) != 0x80) nop();
	g_AnalogReading[8] = ADC_GetConvertedData(AT91C_BASE_ADC0,0);
	g_AnalogReading[9] = ADC_GetConvertedData(AT91C_BASE_ADC0,1);
	g_AnalogReading[10] = ADC_GetConvertedData(AT91C_BASE_ADC0,2);
	g_AnalogReading[11] = ADC_GetConvertedData(AT91C_BASE_ADC0,3);
	//g_AnalogReading[0] = ADC_GetConvertedData(AT91C_BASE_ADC0,4);
	g_AnalogReading[1] = ADC_GetConvertedData(AT91C_BASE_ADC0,5);
	g_AnalogReading[2] = ADC_GetConvertedData(AT91C_BASE_ADC0,6);
	g_AnalogReading[3] = ADC_GetConvertedData(AT91C_BASE_ADC0,7);
	
	ADC_StartConversion(AT91C_BASE_ADC1);
	while((ADC_GetStatus(AT91C_BASE_ADC1) & 0x80) != 0x80) nop();
	g_AnalogReading[4] = ADC_GetConvertedData(AT91C_BASE_ADC1,0);
	g_AnalogReading[5] = ADC_GetConvertedData(AT91C_BASE_ADC1,1);
	g_AnalogReading[6] = ADC_GetConvertedData(AT91C_BASE_ADC1,2);
	g_AnalogReading[7] = ADC_GetConvertedData(AT91C_BASE_ADC1,3);
	g_AnalogReading[12] = ADC_GetConvertedData(AT91C_BASE_ADC1,4);
	g_AnalogReading[13] = ADC_GetConvertedData(AT91C_BASE_ADC1,5);
	g_AnalogReading[14] = ADC_GetConvertedData(AT91C_BASE_ADC1,6);
	g_AnalogReading[15] = ADC_GetConvertedData(AT91C_BASE_ADC1,7);
	
	
	switch(g_BatteryCounter) {
		case 0:
			g_AnalogReading[0] = ADC_GetConvertedData(AT91C_BASE_ADC0, 4);
			SelectBattV();
			break;
		case 1:
			g_BatteryVoltage = ADC_GetConvertedData(AT91C_BASE_ADC0, 4);
			g_BatteryVoltage = (9990*g_BatteryVoltage)/1023;
			SelectAng0();
			break;
		case BATTV_PERIOD:
			g_BatteryCounter = -1;
		default:
			g_AnalogReading[0] = ADC_GetConvertedData(AT91C_BASE_ADC0, 4);
	}
	g_BatteryCounter++;
}

int Analog(int port)
{
	if(port >= 0 && port <= 15) return g_AnalogReading[port];
	else return -1;
}

void AnalogPullUp(int port, int state)
{
	PIO_Set(&(analogPullup[0]));	// 
	
	if(state) PIO_Set(&(analogPullup[1]));	// set data pin HIGH - pullup on
	else PIO_Clear(&(analogPullup[1]));		// set data pin LOW - pullup off
	
	// sets the address pins to the port to enable the data
	if(port & 0x001) PIO_Set(&(analogPullup[2]));
	else PIO_Clear(&(analogPullup[2]));
	if(port & 0x010) PIO_Set(&(analogPullup[3]));
	else PIO_Clear(&(analogPullup[3]));
	if(port & 0x100) PIO_Set(&(analogPullup[4]));
	else PIO_Clear(&(analogPullup[4]));
	
	PIO_Clear(&(analogPullup[0]));	// enable the data on the pin
	nop();
	nop();
	nop();
	PIO_Set(&(analogPullup[0]));	// store the data
}

void SensorPowerOn(void)
{
	PIO_Clear(&(sensorsPower[0]));
}

void SensorPowerOff(void)
{
	PIO_Set(&(sensorsPower[0]));
}

int SensorPowerStatus(void)
{
	return PIO_Get(&(sensorsPower[1]));
}

void SelectBattV(void)
{
	// connects Analog pin 0 to Battery Voltage Divider
	PIO_Set(&(sensorsPower[2]));
}

void SelectAng0(void)
{
	// connects Analog pin 0 to Analog port 0
	PIO_Clear(&(sensorsPower[2]));
}

int BattVoltage(void)
{
	int value;
	SelectBattV();	// switch to Battery voltage resistor network
	usleep(11000);
	
	value = (9990*Analog(0))/1023; // convert analog readings to milivolts
	
	SelectAng0();	// switch back to Analog port 0
	return value;
}
