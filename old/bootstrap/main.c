#include <bob.h>
#include <stdio.h>
#include <usb/usb.h>
#include <sensors/sensors.h>
#include <timer/timer.h>
#include <bootloader/bootloader.h>
#include <motors/motors.h>		
#include <servos/servos.h>
#include <accel/accel.h>
#include <chumby/chumby.h>

int main()
{ 
	//int state = 0;
	//int i=100;
	Bootloader();
	
	//BobInit();
	//ChumbyInit();
	
	//UsbWaitConnect();
	
	//while(!BlackButton()) nop();
	//printf("go to go\r\n");
	//usleep(100000);
	while(1){
		nop();
		/*
		if(BlackButton()){
			//ClearMotorCounter(0);
			//Motor(i,0);
			if(i==100) i=-100;
			else i = 100;
		}
		
		printf("%d\r\n",GetMotorCounter(0));

		MoveAtVelocity(1,i);
		MoveAtVelocity(0,-i);
		//MoveToPosition(0,100,i);
		//Motor(i,50);
		//printf("%d ANG %d\n\r",i,Analog(0));
		usleep(200000);
		 
		*/
	}
	
	//bootloader();
  
	return 1;
}

