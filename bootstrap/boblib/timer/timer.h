#ifndef __TIMER_H__
#define __TIMER_H__

void InitPit(unsigned int msperiod, void (*handler)( void ));
unsigned int GetPITValueReset(void);
unsigned int GetPITValue(void);
void PITInterruptLength(unsigned int ticks);
void usleep(unsigned int usecs);

void TimerInit();
extern volatile unsigned int mseconds;

#endif
