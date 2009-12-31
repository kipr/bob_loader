#include "bob.h"
#include <sensors/sensors.h>
#include <usb/usb.h>
#include <motors/motors.h>
#include <servos/servos.h>
#include <accel/accel.h>

#include <utility/trace.h>
#include <pio/pio_it.h>

void BobInit()
{
	TRACE_CONFIGURE(DBGU_STANDARD, 115200, MCK);
	PIO_InitializeInterrupts(0);
	UsbInit();
	SensorsInit();
	MotorsInit();
	ServosInit();
	AccelInit();
}
