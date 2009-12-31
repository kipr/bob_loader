#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char **argv)
{
	int stream;
	struct termios tio;
	char dataIn[3];
	int count, i;
	FILE *file, *dump;
	char data[262];
	unsigned long crc;
	
	if(argc < 3) {
		printf("Usage: %s <firmware image> <serial port>\n", argv[0]);
		return 1;
	}
	
	stream = open(argv[2], O_NDELAY | O_RDWR | O_NOCTTY);
	if(stream < 0) {
		perror("Error opening serial port");
		return 1;
	}
	//fcntl(stream, F_SETFL, ~O_NDELAY & fcntl(stream, F_GETFL, 0));
	
	tcgetattr(stream, &tio);
	
	tio.c_iflag = IGNBRK;
	tio.c_oflag = 0;
	tio.c_cflag = CREAD | CLOCAL;
	tio.c_cflag |= CS8;
	tio.c_lflag = 0;
	bzero(&tio.c_cc, sizeof(tio.c_cc));
	
	tio.c_cc[VTIME] = 0;
	tio.c_cc[VMIN] = 0;
	
	cfsetspeed(&tio, B115200);
	
	tcsetattr(stream, TCSADRAIN, &tio);
	
	file = fopen(argv[1], "r");
	
	if(!file) {
		perror("Error opening firmware file");
		return 1;
	}
	
  data[0] = 'W'; data[1] = 'R';
  count = fread(data+2, 1, 256, file);
  while(count > 0) {
    memset(data+2+count, 0, 262-count);
    crc = crc32(data, 258);
    memcpy(data+258, &crc, 4);
		
    while(!(dataIn[0] == 'O' && dataIn[1] == 'K')){
      write(stream, data, 262);
      count = 0;
      while(count < 2)
        count += read(stream, dataIn+count, 2-count);
			printf("ret=%.2hhx%.2hhx\n", dataIn[0], dataIn[1]);
    }
    printf("Got OK for 256 bytes\n");
    count = fread(data+2, 1, 256, file);
  }
  
  write(stream, "OK", 2);
  while(read(stream, dataIn, 1) > 0)
    printf("%c", dataIn[0]);
  printf("\n\n");
	close(stream);
	fclose(file);
	
  return 0;
}
