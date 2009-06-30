#ifndef __MOTORS_H__
#define __MOTORS_H__

void MotorsInit();
int MotorCalibration(void);
void MoveToPosition(int motor, int speed, int position);
void MoveAtVelocity(int motor, int speed);
void Motor(int port, int power);
int GetMotor(int port);
int GetMotorCounter(int motor);
int ClearMotorCounter(int motor);
int BlockMotorDone(int motor);
int IsMotorDone(int motor);
void GetPIDGains(short *PM, short *IM, short *DM, short *PD, short *ID, short *DD);
void SetPIDGains(short PM, short IM, short DM, short PD, short ID, short DD);
#endif
