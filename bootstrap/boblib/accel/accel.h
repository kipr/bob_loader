

unsigned char AccelInit(void);

int Accel_X(void);
int Accel_Y(void);
int Accel_Z(void);

unsigned char AccelSetScale(int gsel);

unsigned char WriteAccelData(unsigned int iaddress, char *bytes, unsigned int num);
unsigned char ReadAccelData(unsigned int iaddress, char *bytes, unsigned int num);
