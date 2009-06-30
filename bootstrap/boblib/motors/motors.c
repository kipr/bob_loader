#include "motors.h"

#include <bob.h>
#include <pio/pio.h>
#include <pmc/pmc.h>
#include <pwmc/pwmc.h>
#include <timer/timer.h>
#include <adc/adc.h>
#include <sensors/sensors.h>

#define M0_LEFT_ADC
#define M1_LEFT_ADC
#define M2_LEFT_ADC
#define M3_LEFT_ADC

#define M0_RIGHT_ADC
#define M1_RIGHT_ADC
#define M2_RIGHT_ADC
#define M3_RIGHT_ADC

Pin g_Motor[] = {MC_PWM};
Pin g_MotorOff[] = {MC_PWM_OFF};
Pin g_MotorDirection[] = {MC_DIR};

volatile int g_MotorThreshold[3] = {100,0,1000};// Position, Velocity, Acceleration thresholds
volatile int g_BackEMF[4] = {0,0,0,0};			// Motor Back-EMF readings
volatile int g_MotorCal[4] = {0,0,0,0};			// Motor calibration 
volatile int g_MotorPWM[4] = {0,0,0,0};			// PWM duty cycle of the motor
volatile int g_MotorCounter[4] = {0,0,0,0};		// Motor position counter
volatile int g_MotorTPC[4] = {0,0,0,0};			// Ticks Per Cycle of the motor
volatile int g_MotorInMotion;					// first 4 bits indicate motor is in Back-EMF control

volatile int g_TargetTPC[4] = {0,0,0,0};		// Motor target Ticks per Cycle
volatile int g_LastTargetTPC[4] = {0,0,0,0};
volatile int g_TargetPosition[4] = {0,0,0,0};	// Motor target position for move to position command
volatile int g_LastTargetPosition[4] = {0,0,0,0};
volatile int g_NextPosition[4] = {0,0,0,0};		// Future position estimate based on current speed 
volatile int g_MotorError[4] = {0,0,0,0};		// Accumulated speed error for Integral term
volatile int g_MotorLastError[4] = {0,0,0,0};	// Previous speed error for Derivative term
volatile int g_MotorGains[6] = {7,0,-2,20,30,1};// Proportional Mult,Integral Mult,Derivative Mult,
												// Proportional Divi,Integral Divi,Derivative Divi
static void ISR_Pit(void);
void UpdateBEMF(void);
int MotorControl(int motor);
int PID(int motor, int err);
void SetPWM(int motor, int pwm);
void AllOn(void);
void AllOff(void);

// Sensors and Timers must be initialized first!!!
void MotorsInit()
{
	int i=0;
	PIO_Configure(g_MotorOff, PIO_LISTSIZE(g_Motor));
	PIO_Configure(g_MotorDirection, PIO_LISTSIZE(g_MotorDirection));
	
	// enable the PWM peripheral if it has not already been
	if(!PMC_IsPeriphEnabled(AT91C_ID_PWMC)) PMC_EnablePeripheral(AT91C_ID_PWMC);

	PWMC_ConfigureClocks(1000000, 0, MCK);	
	
	PWMC_ConfigureChannel(4, AT91C_PWMC_CPRE_MCK, 0, 0);
	PWMC_ConfigureChannel(5, AT91C_PWMC_CPRE_MCK, 0, 0);
	PWMC_ConfigureChannel(6, AT91C_PWMC_CPRE_MCK, 0, 0);
	PWMC_ConfigureChannel(7, AT91C_PWMC_CPRE_MCK, 0, 0);
	
	PWMC_SetPeriod(4, MC_PWM_PERIOD);
	PWMC_SetPeriod(5, MC_PWM_PERIOD);
	PWMC_SetPeriod(6, MC_PWM_PERIOD);
	PWMC_SetPeriod(7, MC_PWM_PERIOD);
	
	PWMC_EnableChannel(4);
	PWMC_EnableChannel(5);
	PWMC_EnableChannel(6);
	PWMC_EnableChannel(7);

	InitPit(MC_PID_PERIOD,ISR_Pit);
	
	// make sure the motors calibrate properly, DO NOT SPIN MOTORS DURING CALIBRATION !!
	while(i < 5){
		if(MotorCalibration()){ 
			usleep(100000);
			i++;
		}
		else break;
	}
	
}

// sets the PWM of a motor in port 0-3 with a PWM of +/- MC_PWM_PERIOD (240)
void SetPWM(int motor, int pwm)
{	
	if(pwm == 0) {
		PIO_Configure(&(g_MotorOff[motor]), 1);
		return;
	}
	// else configure the pin to use the PWM controller
	PIO_Configure(&(g_Motor[motor]), 1);
	// if the PWM is negative flip the direction pin
	if(pwm < 0) {
		PIO_Clear(&(g_MotorDirection[motor]));
		pwm = -pwm;
	}
	else
		PIO_Set(&(g_MotorDirection[motor]));
	
	
	// A period less than or equal to 1 does not turn on the PWM
	if(pwm > (MC_PWM_PERIOD-2)) pwm = MC_PWM_PERIOD-2;
	pwm = MC_PWM_PERIOD - pwm;
	// set the duty cycle of the PWM of the motor
	PWMC_SetDutyCycle(motor+4, pwm);
}

// Turns off all motors
void AllOff(void)
{
	PIO_Configure(g_MotorOff, PIO_LISTSIZE(g_Motor));
}

// Turns on all motors to the PWM value remaining in the Duty Cycle register
void AllOn(void)
{
	PIO_Configure(g_Motor, PIO_LISTSIZE(g_Motor));
}	

void UpdateBEMF(void)
{
	UpdateAnalogs();
		
	/* take the difference between the BEMF readings to reduce error 
	g_BackEMF[0] = ADC_GetConvertedData(AT91C_BASE_ADC0, 0);
	g_BackEMF[0] -= ADC_GetConvertedData(AT91C_BASE_ADC0, 1);
	g_BackEMF[1] = ADC_GetConvertedData(AT91C_BASE_ADC0, 2);
	g_BackEMF[1] -= ADC_GetConvertedData(AT91C_BASE_ADC0, 3);
	g_BackEMF[2] = ADC_GetConvertedData(AT91C_BASE_ADC1, 5);
	g_BackEMF[2] -= ADC_GetConvertedData(AT91C_BASE_ADC1, 4);
	g_BackEMF[3] = ADC_GetConvertedData(AT91C_BASE_ADC1, 7);
	g_BackEMF[3] -= ADC_GetConvertedData(AT91C_BASE_ADC1, 6);
	*/
	g_BackEMF[0] = Analog(8) - Analog(9);
	g_BackEMF[1] = Analog(10) - Analog(11);
	g_BackEMF[2] = Analog(13) - Analog(12);
	g_BackEMF[3] = Analog(15) - Analog(14);
	
	// correct any bias using the calibration values
	g_BackEMF[0] -= g_MotorCal[0];
	g_BackEMF[1] -= g_MotorCal[1];
	g_BackEMF[2] -= g_MotorCal[2];
	g_BackEMF[3] -= g_MotorCal[3];
}

int MotorCalibration(void)
{
	int i,j;
	int bias[] = {0,0,0,0};
	
	for(i=0;i<4;i++) g_MotorCal[i] = 0;
	
	AllOff();	// turn off all motors
	g_MotorInMotion = 0;
	usleep(15000);
	for(i=0;i<8;i++){
		for(j=0;j<4;j++) bias[j] += g_BackEMF[j];
		usleep(15000);
	}
	
	for(i=0;i<4;i++) g_MotorCal[i] = bias[i]/8;
	
	// if any motor calibration is out of bounds return the motor acting up
	for(i=0;i<4;i++) if(g_MotorCal[i] > 5 || g_MotorCal[i] < -5) return i;
	// otherwhise all motors are normal
	return 0;
}

static void ISR_Pit(void)
{
	static int i;
	unsigned int time;
	
	time = GetPITValueReset();

	AllOff();							// turn off motors
	time = GetPITValueReset() + 3*MC_INDUCTION_SPIKE;	// PIT clock runs at 1/3 of a micro second
	while(time > GetPITValue()) nop();	// wait for induction spike to settle
	UpdateBEMF();			// read in back EMF analog readings
	//AllOn();				// turn motors back on while calculating new speeds
	
	// update all counters and speeds
	for(i=0;i<4;i++){
		if(g_BackEMF[i] > 3 || g_BackEMF[i] < -3){
			g_MotorTPC[i] = g_BackEMF[i];
			g_MotorCounter[i] += g_BackEMF[i];
		}
		else g_MotorTPC[i] = 0;
	}
	
	// cycle through all 4 motors
	for(i=0;i<4;i++){
		// if the Target Ticks-per-Cycle is set then we are doing PID control
		if(g_TargetTPC[i] != 0) {
			// reset accumulated error for integral term if motor target settings change
			if((g_LastTargetTPC[i] != g_TargetTPC[i]) || (g_LastTargetPosition[i] != g_TargetPosition[i])) {
				g_MotorError[i] = 0;
				g_NextPosition[i] = g_MotorCounter[i];
				g_MotorInMotion |= (1<<i);
				g_LastTargetTPC[i] = g_TargetTPC[i];
				g_LastTargetPosition[i] = g_TargetPosition[i];
			}
			
			g_MotorPWM[i] = MotorControl(i);	// compute PID loop
		}
		// we are just setting the PWM
		else {																	
			g_MotorInMotion &= ~(1<<i);			// motor is not in BEMF	motion
			g_LastTargetTPC[i] = 0;
		}
		
		// update motors with new speeds
		SetPWM(i,g_MotorPWM[i]);
	}
	
	time = GetPITValueReset();
	PITInterruptLength(time);
}

int MotorControl(int motor)
{	
	int diffPosition = g_TargetPosition[motor] - g_MotorCounter[motor];
	
	// check if the difference in position is less than the next loop cycle based on motor speed
	if((diffPosition > -g_TargetTPC[motor]) && (diffPosition < g_TargetTPC[motor])){
		
		// is the difference in position within the threshold values
		if((diffPosition > -g_MotorThreshold[0]) && (diffPosition < g_MotorThreshold[0])) {
			g_NextPosition[motor] = g_MotorCounter[motor];	// set zero error for PID loop
			g_MotorInMotion &= ~(1<<motor);					// clear the Motor in motion bit
		}
		// the next position will only be the difference to the target 
		else g_NextPosition[motor] += diffPosition;
		
	}
	// the next position will increased by the current motor speed
	else if(diffPosition > 0) g_NextPosition[motor] += g_TargetTPC[motor];
	else g_NextPosition[motor] -= g_TargetTPC[motor];
	
	// using the difference between the probable future position and the current position
	// compute the PID for speed control
	return PID(motor, g_NextPosition[motor] - g_MotorCounter[motor]);
}

int PID(int motor, int err)
{
	int Pterm, Iterm, Dterm,PIDterm;
	
	// is the error within the maximum or minimum motor power range?
	if((g_MotorError[motor] > 170000) || (g_MotorError[motor] < -170000)) g_MotorError[motor] = 170000;
	else g_MotorError[motor] += err;
	
	// Proportional Term
	Pterm = (g_MotorGains[0] * err)/g_MotorGains[3];
	// Integral Term
	Iterm = (g_MotorGains[1] * g_MotorError[motor])/g_MotorGains[4];
	// Derivative Term
	Dterm = (g_MotorGains[2] * (g_MotorLastError[motor] - err))/g_MotorGains[5];
	
	g_MotorLastError[motor] = err;
	
	PIDterm = Pterm + Iterm + Dterm;
	
	if(PIDterm > MC_PWM_PERIOD) return MC_PWM_PERIOD;
	else if(PIDterm < -MC_PWM_PERIOD) return -MC_PWM_PERIOD;
	else return PIDterm;
}

void GetPIDGains(short *PM, short *IM, short *DM, short *PD, short *ID, short *DD)
{
	*PM = g_MotorGains[0];
	*IM = g_MotorGains[1];
	*DM = g_MotorGains[2];
	*PD = g_MotorGains[3];
	*ID = g_MotorGains[4];
	*DD = g_MotorGains[5];
}

void SetPIDGains(short PM, short IM, short DM, short PD, short ID, short DD)
{
	g_MotorGains[0] = PM;
	g_MotorGains[1] = IM;
	g_MotorGains[2] = DM;
	g_MotorGains[3] = PD;
	g_MotorGains[4] = ID;
	g_MotorGains[5] = DD;
}

void MoveToPosition(int motor, int speed, int position)
{	
	if(speed < 0) speed = -speed;
	// for black modded servo motors 1 rev = 38750 ticks
	g_TargetTPC[motor] = speed;
	g_TargetPosition[motor] = position;
}

void MoveAtVelocity(int motor, int speed)
{
	// set target position to maximum distance or 2^32/4 (dealing with signed ints)
	// for black modded servo motors 1 rev = 38750 ticks
	// this should give a black servo motor about 7 hours of running at 1rev/sec
	int distance = 1073741823;
	if(speed > 0) {
		g_TargetPosition[motor] = distance;
		g_TargetTPC[motor] = speed;
	}
	else{
		g_TargetPosition[motor] = -distance;
		g_TargetTPC[motor] = -speed;
	}
}

int BlockMotorDone(int motor)
{
	while(g_MotorInMotion & (1<<motor)) nop();
	return 1;
}

// returns 0 if motor is still moving to position
// returns 1 if the motor has reached the position since the last MTP command
int IsMotorDone(int motor)
{
	if(g_MotorInMotion & (1<<motor)) return 0;
	else return 1;
}

int GetMotorCounter(int motor)
{
  return g_MotorCounter[motor];
}

int ClearMotorCounter(int motor)
{
	g_MotorCounter[motor] = 0;
	g_MotorError[motor] = 0;
	g_NextPosition[motor] = 0;
	g_LastTargetPosition[motor] = 0;
	return g_MotorCounter[motor];
}

void Motor(int port, int power)
{	
	if(port > 3 || port < 0) return;
	
	g_TargetTPC[port] = 0;
	
	g_MotorPWM[port] = MC_PWM_PERIOD*power/100;	// re-scale the motor power to pwm settings
}

int GetMotor(int port)
{
	int power;
	power = (g_MotorPWM[port]*100)/MC_PWM_PERIOD;
	return power;
}
