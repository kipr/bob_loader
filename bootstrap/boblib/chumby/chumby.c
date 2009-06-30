#include "chumby.h"

#include <pio/pio.h>
#include <usb/usb.h>
#include <stdio.h>

#include <sensors/sensors.h>
#include <motors/motors.h>
#include <accel/accel.h>
#include <uart/uart.h>
#include <string.h>

#include "cbob_cmd.h"
#include "chumby_spi.h"

//typedef unsigned short
typedef unsigned char uchar;

static Pin g_ChumbyPins[] = {CHUMBY};
static Pin g_ChumbyBend = CHUMBY_BEND;
 
volatile short g_ChumbyCmd[CHUMBY_CMD_COUNT];
volatile short g_ChumbyData[CHUMBY_MAX_DATA_COUNT];
volatile uchar  g_ChumbyState = -1;

static void ChumbyCallback();

static void ChumbySetStateCmd();
static void ChumbySetStateData(short count);
static void ChumbyHandleCmd(short cmd, short *data, short count);
static void ChumbySetStateWriteData();
static void ChumbySetStateWriteLen(short count);

void ChumbyInit()
{
  PIO_Configure(g_ChumbyPins, PIO_LISTSIZE(g_ChumbyPins));
  
  ChumbySpiInit();
  ChumbySpiSetCallback(ChumbyCallback);
  
  ChumbySetStateCmd();
}

static void ChumbySetStateCmd()
{
  ChumbySpiReset();
  
  g_ChumbyState = CHUMBY_STATE_RXT_CMD;
  g_ChumbyCmd[0] = 0;
  g_ChumbyCmd[1] = 0;
  g_ChumbyCmd[2] = 0 ;
  
  ChumbySpiRead((void*)g_ChumbyCmd, CHUMBY_CMD_COUNT);
  
  ChumbyBend(0);
}

static void ChumbySetStateData(short count)
{
  ChumbySpiRead((void*)g_ChumbyData, count);
  
  g_ChumbyState = CHUMBY_STATE_RXT_DATA;
  ChumbyBend(1);
}

static void ChumbySetStateWriteLen(short count)
{
  int i=0;
  if(count < 1) {
    count = 1;
    g_ChumbyData[1] = 0;
  }

  g_ChumbyData[0] = count;
  ChumbySpiWrite((void *)g_ChumbyData, 2);
  // BUG: Why does this only work with
  //      count = 2?  Something is broken
  //      When count = 1, ChumbyBend(0)
  //      Has no effect. WEIRD!
  
  g_ChumbyState = CHUMBY_STATE_TXT_LEN;
  
  while(PIO_GetOutputDataStatus(&g_ChumbyBend))
  {
    ChumbyBend(0);
    i++;
  }
    
  printf("loop=%d", i);
}

static void ChumbySetStateWriteData()
{
  // Also really weird. The TX circuitry on this chip
  // Is totally fubared.  Oh well
  ChumbySpiWrite((void *)&(g_ChumbyData[1]), g_ChumbyData[0]+1);
  
  g_ChumbyState = CHUMBY_STATE_TXT_DATA;
  ChumbyBend(1);
}

static void ChumbyHandleCmd(short cmd, short *data, short length)
{
  int tmp, outcount = 0;
	
	switch(cmd) {
    case CBOB_CMD_DIGITAL_READ:
      if(data[0] >= 0 && data[0] <= 7)
        g_ChumbyData[1] = Digital(data[0]);
      else if(data[0] == 8)
        g_ChumbyData[1] = BlackButton();
      else
        g_ChumbyData[1] = AllDigitals() | (BlackButton()<<8);
      outcount = 1;
			break;
		case CBOB_CMD_DIGITAL_WRITE:
			/*if(data[0] >= 0 && data[0] <= 7) {
				DigitalOut(data[0], data[1]);
			}
			else {
				for(tmp = 0;tmp < 8;tmp++) {
					DigitalOut(tmp, (data[1] & (1<<tmp) ? 1 : 0));
				}
			}*/
		case CBOB_CMD_DIGITAL_CONFIG:
			break;
    case CBOB_CMD_ANALOG_READ:
			if(data[0] >= 0 && data[0] < 8) {
				g_ChumbyData[1] = Analog(data[0]);
				outcount = 1;
			}
			else if(data[0] == 8) {
				g_ChumbyData[1] = BattVoltage();
			}
			else {
				for(outcount = 1;outcount <= 8;outcount++) {
					g_ChumbyData[outcount] = Analog(outcount-1);
				}
				g_ChumbyData[outcount] = BattVoltage();
			}
      break;
		case CBOB_CMD_ACCEL_READ:
			if(data[0] == 0) {
				g_ChumbyData[1] = Accel_X();
				outcount = 1;
			}
			else if(data[0] == 1) {
				g_ChumbyData[1] = Accel_Y();
				outcount = 1;
			}
			else if(data[0] == 2) {
				g_ChumbyData[1] = Accel_Z();
				outcount = 1;
			}
			else {
				g_ChumbyData[1] = Accel_X();
				g_ChumbyData[2] = Accel_Y();
				g_ChumbyData[3] = Accel_Z();
				outcount = 3;
			}
			break;
		case CBOB_CMD_ACCEL_CONFIG:
			break;
    case CBOB_CMD_SENSORS_READ:
      g_ChumbyData[1] = AllDigitals() | (BlackButton()<<8);
      for(outcount = 2;outcount <= 9;outcount++) {
				g_ChumbyData[outcount] = Analog(outcount-2);
			}
			g_ChumbyData[outcount] = 0;
			g_ChumbyData[++outcount] = Accel_X();
			g_ChumbyData[++outcount] = Accel_Y();
			g_ChumbyData[++outcount] = Accel_Z();
      break;
    case CBOB_CMD_SENSORS_CONFIG:
			break;
    case CBOB_CMD_PWM_READ:
			if(data[0] >= 0 && data[0] < 4) {
				data[1] = GetMotor(data[0]);
				outcount = 1;
			}
			else {
				data[1] = GetMotor(0);
				data[2] = GetMotor(1);
				data[3] = GetMotor(2);
				data[4] = GetMotor(3);
				outcount = 4;
			}
      break;
    case CBOB_CMD_PWM_WRITE:
      if(data[0] <= 3) {
        Motor(data[0], data[1]);
      }
      else {
        Motor(0, data[1]);
        Motor(1, data[2]);
        Motor(2, data[3]);
        Motor(3, data[4]);
      }
      break;
		case CBOB_CMD_PWM_CONFIG:
			break;
		case CBOB_CMD_PID_READ:
			if(data[0] <= 3) {
				tmp = GetMotorCounter(data[0]);
				memcpy(((void*)data)+2, &tmp, 4);
				outcount = 2;
			}
			else {
				tmp = GetMotorCounter(0);
				memcpy(((void*)data)+2, &tmp, 4);
				tmp = GetMotorCounter(1);
				memcpy(((void*)data)+6, &tmp, 4);
				tmp = GetMotorCounter(2);
				memcpy(((void*)data)+10, &tmp, 4);
				tmp = GetMotorCounter(3);
				memcpy(((void*)data)+14, &tmp, 4);
				outcount = 8;
			}
			break;
		case CBOB_CMD_PID_WRITE:
			if(data[0] >= 0 && data[0] < 4) {
				if(length > 2) {
					memcpy(&tmp, ((void*)data)+4, 4);
					MoveToPosition(data[0], data[1], tmp);
				}
				else {
					MoveAtVelocity(data[0], data[1]);
				}
			}
			break;
		case CBOB_CMD_PID_CONFIG:
			break;
		case CBOB_CMD_SERVO_READ:
			if(data[0] >= 0 && data[0] < 4) {
				data[1] = GetServoPosition(data[0]);
				outcount = 1;
			}
			else {
				data[1] = GetServoPosition(0);
				data[2] = GetServoPosition(1);
				data[3] = GetServoPosition(2);
				data[4] = GetServoPosition(3);
				outcount = 4;
			}
		case CBOB_CMD_SERVO_WRITE:
			if(data[0] >= 0 && data[0] < 4) {
				SetServoPosition(data[0], data[1]);
			}
		case CBOB_CMD_SERVO_CONFIG:
			break;
		case CBOB_CMD_UART_READ:
			//outcount = (UartRead((data[0]>>8)&0xff,(char*)(&(data[1])), data[0]&0xff)>>1);
			break;
		case CBOB_CMD_UART_WRITE:
		case CBOB_CMD_UART_CONFIG:
			break;
  }
  ChumbySetStateWriteLen(outcount);
}

void ChumbyBend(int value)
{
  if(value) {
    PIO_Set(&g_ChumbyBend);
  }
  else {
    PIO_Clear(&g_ChumbyBend);
  }
}

static void ChumbyCallback()
{
  switch(g_ChumbyState) {
    case CHUMBY_STATE_RXT_CMD:
      if((ushort)g_ChumbyCmd[0] == CHUMBY_START) {
        ChumbySetStateData(g_ChumbyCmd[2]);
      }
      else {
        ChumbySetStateCmd();
      }
      return;
    case CHUMBY_STATE_RXT_DATA:
      ChumbyHandleCmd(g_ChumbyCmd[1], (short*)g_ChumbyData, g_ChumbyCmd[2]);
      return;
    case CHUMBY_STATE_TXT_DATA:
      ChumbySetStateCmd();
      return;
    case CHUMBY_STATE_TXT_LEN:
      ChumbySetStateWriteData();
      return;
  }
}
