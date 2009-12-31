#ifndef __CHUMBY_SPI_H__
#define __CHUMBY_SPI_H__

void ChumbySpiInit();
void ChumbySpiReset();

void ChumbySpiWrite(void *data, int len);
void ChumbySpiRead (void *data, int len);

#endif