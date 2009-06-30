#include "accel.h"

#include <bob.h>
#include <pio/pio.h>
#include <pmc/pmc.h>
#include <twi/twi.h>
#include <timer/timer.h>

// Accelerometer TWI address
#define ACCELADDR	0X1D
#define TWITIMEOUTMAX 50000
// Pin Setup Arrays
const Pin twiPins[] = {PIN_TWID,PIN_TWICK};

// Data buffer
static char aData[6];

unsigned char AccelInit(void)
{
	PIO_Configure(twiPins, PIO_LISTSIZE(twiPins));
	PMC_EnablePeripheral(AT91C_ID_TWI);
	
	TWI_ConfigureMaster(AT91C_BASE_TWI,TWCK,MCK);
	
	usleep(100);
	if(AccelSetScale(2)) return 1;	// set accelerometer to 2g scale
	
	return 0;
}

int Accel_X(void)
{
	int value;
	ReadAccelData(0x00, aData, 2);
	value = aData[1]<<8 | aData[0];
	if(value > 511) value -= 1024;
	return value;
}

int Accel_Y(void)
{
	int value;
	ReadAccelData(0x02, aData, 2);
	value = aData[1] << 8 | aData[0];
	if(value > 511) value -= 1024;
	return value;
}

int Accel_Z(void)
{
	int value;
	ReadAccelData(0x04, aData, 2);
	value = aData[1] << 8 | aData[0];
	if(value > 511) value -= 1024;
	return value;
}

// sets the g-scale of the chip 2,4,8 g measurment
unsigned char AccelSetScale(int gsel)
{
	int scale;
	
	switch (gsel) {
		case 2:
			scale = 1;
			break;
		case 4:
			scale = 2;
			break;
		case 8:
			scale = 0;
			break;
		default:
			scale = 2;
			break;
	}
	aData[0] = (scale << 2) | 1; // set scale and turn on measurement mode
	return WriteAccelData(0x16, aData, 1);
}

unsigned char WriteAccelData(unsigned int iaddress, char *bytes, unsigned int num)
{
	unsigned int timeout;
	
	// wait for TWI bus to be ready
	while(!(TWI_TransferComplete(AT91C_BASE_TWI))) nop();
	
	// Start Writing
	TWI_StartWrite(AT91C_BASE_TWI,ACCELADDR,iaddress,1,*bytes++);
	num--;
	
	while(num > 0){
		// Wait before sending the next byte
		timeout = 0;
		while(!TWI_ByteSent(AT91C_BASE_TWI) && (++timeout<TWITIMEOUTMAX)) nop();
		if(timeout == TWITIMEOUTMAX) return 1;
		TWI_WriteByte(AT91C_BASE_TWI, *bytes++);
		num--;
	}
	
	return 0;
}

unsigned char ReadAccelData(unsigned int iaddress, char *bytes, unsigned int num)
{
	unsigned int timeout;
	
	// wait for TWI bus to be ready
	while(!(TWI_TransferComplete(AT91C_BASE_TWI))) nop();
	
	// Start Reading
	TWI_StartRead(AT91C_BASE_TWI, ACCELADDR,iaddress,1);
	
	while (num > 0) {
		// Last byte
		if(num == 1) TWI_Stop(AT91C_BASE_TWI);
		
		// wait for byte then read and store it
		timeout = 0;
		while(!TWI_ByteReceived(AT91C_BASE_TWI) && (++timeout<TWITIMEOUTMAX)) nop();
		if(timeout == TWITIMEOUTMAX) return 2;
		*bytes++ = TWI_ReadByte(AT91C_BASE_TWI);
		num--;
	}
	
	return 0;
}
