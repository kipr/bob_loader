#include "timer.h"

#include <bob.h>
#include <pmc/pmc.h>
#include <tc/tc.h>
#include <aic/aic.h>
#include <pio/pio_it.h>
#include <pit/pit.h>
#include <sensors/sensors.h>

volatile unsigned int mseconds = 0;
volatile unsigned int InterruptTicks = 0;

//static void ISR_Pit(void);
static void ISR_TC0();

void InitPit(unsigned int msperiod, void (*handler)( void ))
{
	PIT_DisableIT();
    // Initialize the PIT to the desired frequency
    PIT_Init(0, 0);
	// PIT timer runs at MCK/16 
	// calculates the PIT Value accurate to a Millisecond interrupt
	// msperiod can not be larget than 349 because PIV is at a 20bit limit
	if(msperiod > 349) msperiod = 349;
    PIT_SetPIV((MCK/(16*1000))*msperiod);
	
    // Configure interrupt on PIT
    AIC_DisableIT(AT91C_ID_SYS);
    AIC_ConfigureIT(AT91C_ID_SYS, AT91C_AIC_PRIOR_LOWEST, handler);
    AIC_EnableIT(AT91C_ID_SYS);
    PIT_EnableIT();
	
    // Enable the pit
    PIT_Enable();
}

unsigned int GetPITValue(void)
{
	unsigned int value;
	value = PIT_GetPIIR();
	return value;
}

unsigned int GetPITValueReset(void)
{
	unsigned int value;
	value = PIT_GetPIVR();
	return value;
}

void PITInterruptLength(unsigned int ticks)
{
	InterruptTicks = ticks;
}

void usleep(unsigned int usecs)
{
	unsigned int waitTicks;
	unsigned int maxTicks = (PIT_GetMode() & 0xFFFFF);
	
	// get the time the wait should end, PIT clock tick is 1/3 of a microsecond
	waitTicks = PIT_GetPIIR() + usecs*3;
	
	// loop incase wait time is longer than multiple interrupts
	while(waitTicks > maxTicks){
		// subtract the time between interrupts off the wait time
		waitTicks -= maxTicks;
		
		InterruptTicks = 0;
		// wait until the interrupt has been called, it will update InterruptTicks with its length
		while(InterruptTicks == 0) nop();
		// if the wait time is over during the interrupt then return
		if(waitTicks < InterruptTicks) return;
		// else take the length of the interrupt into account
		waitTicks -= InterruptTicks;
	}
	// if ther is enought time just wait then exit
	while(waitTicks >= PIT_GetPIIR()) nop();
}

void TimerInit()
{
	PMC_EnablePeripheral(AT91C_ID_TC0);				// enable the Timer Counter from PMC
	TC_Configure(AT91C_BASE_TC0, AT91C_TC_CPCTRG);	// Configure TC-0 with RC compare trig.
	AT91C_BASE_TC0->TC_RC = 24000;					// Interrupt every 0.001s
	TC_Start(AT91C_BASE_TC0);							// start the timer
	
	AIC_ConfigureIT(AT91C_ID_TC0, AT91C_AIC_PRIOR_LOWEST, ISR_TC0);
	AT91C_BASE_TC0->TC_IER = AT91C_TC_CPCS;
	AIC_EnableIT(AT91C_ID_TC0);
}

static void ISR_TC0()
{
	AT91C_BASE_TC0->TC_SR;
	mseconds++;
}
