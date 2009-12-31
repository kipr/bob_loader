#ifndef __SENSORS_H__
#define __SENSORS_H__

void SensorsInit();

int BlackButton();
int AllDigitals();
int Digital(int port);
void DigitalOut(int port, int state);
void DigitalConfig(int port, int dir);
int GetDigitalConfig(int port);

void UpdateAnalogs(void);
int Analog(int port);
void AnalogPullUp(int port, int state);

void SensorPowerOn(void);
void SensorPowerOff(void);
int SensorPowerStatus(void);

void SelectBattV(void);
void SelectAng0(void);

int BattVoltage(void);
#endif
