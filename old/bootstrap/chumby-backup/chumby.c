#include "chumby.h"

#include <pio/pio.h>
#include <pmc/pmc.h>
#include <aic/aic.h>
#include <usb/usb.h>
#include <stdio.h>

#include "cbob_cmd.h"

//typedef unsigned short ushort;
typedef unsigned char uchar;

static Pin g_ChumbyPins[] = {CHUMBY};
static Pin g_ChumbyBend = CHUMBY_BEND;
 
volatile ushort g_ChumbyCmd[CHUMBY_CMD_COUNT];
volatile ushort g_ChumbyData[CHUMBY_MAX_DATA_COUNT];
volatile uchar  g_ChumbyState = -1;

static void ChumbyISR();

static void ChumbySetStateCmd();
static void ChumbySetStateData(ushort count);
static void ChumbyHandleCmd(ushort cmd, ushort *data, ushort count);
static void ChumbySetStateWriteData();
static void ChumbySetStateWriteLen(ushort count);

static void dumpRegisters()
{
  printf("PDC_TPR=%x,PDC_TCR=%x\n\rg_ChumbyCmd=[%x,%x,%x]\n\rstate=%d\n\r\n\r",
    CHUMBY_DMA->PDC_TPR,CHUMBY_DMA->PDC_TCR,g_ChumbyCmd[0],g_ChumbyCmd[1],g_ChumbyCmd[2],g_ChumbyState);
}

static void ChumbyTXEN()
{
  CHUMBY_DMA->PDC_PTCR = AT91C_PDC_TXTEN | AT91C_PDC_RXTDIS;
}

static void ChumbyRXEN()
{
  CHUMBY_DMA->PDC_PTCR = AT91C_PDC_TXTDIS | AT91C_PDC_RXTEN;
}

static void ChumbyRST()
{
  CHUMBY_SPI->SPI_CR = AT91C_SPI_SWRST;
  CHUMBY_SPI->SPI_CR = AT91C_SPI_SPIDIS;
  CHUMBY_SPI->SPI_MR = 0;
  CHUMBY_SPI->SPI_CSR[0] = AT91C_SPI_CPOL | AT91C_SPI_BITS_16;
  CHUMBY_SPI->SPI_CR = AT91C_SPI_SPIEN;
  CHUMBY_SPI->SPI_IDR = AT91C_SPI_ENDRX | AT91C_SPI_ENDTX;
}

void ChumbyInit()
{
  // These routines will turn on the SPI Bus
  PIO_Configure(g_ChumbyPins, PIO_LISTSIZE(g_ChumbyPins));
  
  PMC_EnablePeripheral(AT91C_ID_SPI0);
  /*CHUMBY_SPI->SPI_MR = 0;
  CHUMBY_SPI->SPI_CSR[0] = AT91C_SPI_CPOL;
  CHUMBY_SPI->SPI_CR = AT91C_SPI_SPIEN;*/
  ChumbyRST();
  
  CHUMBY_DMA->PDC_PTCR = AT91C_PDC_TXTEN | AT91C_PDC_RXTEN;
  
  AIC_ConfigureIT(AT91C_ID_SPI0, AT91C_AIC_PRIOR_LOWEST, ChumbyISR);
  AIC_EnableIT(AT91C_ID_SPI0);
  
  ChumbySetStateCmd();
  
  dumpRegisters();
}

static void ChumbyISR()
{
  dumpRegisters();
  switch(g_ChumbyState) {
    case CHUMBY_STATE_RXT_CMD:
      if(g_ChumbyCmd[0] == CHUMBY_START) {
        ChumbySetStateData(g_ChumbyCmd[2]);
      }
      else {
        ChumbySetStateCmd();
      }
      return;
    case CHUMBY_STATE_RXT_DATA:
      ChumbyHandleCmd(g_ChumbyCmd[1], g_ChumbyData, g_ChumbyCmd[2]);
      return;
    case CHUMBY_STATE_TXT_DATA:
      ChumbySetStateCmd();
      return;
    case CHUMBY_STATE_TXT_LEN:
      ChumbySetStateWriteData();
      return;
  }
}

static void ChumbySetStateCmd()
{
  ChumbyRST();
  CHUMBY_DMA->PDC_TPR = 0;
  CHUMBY_DMA->PDC_RPR = (unsigned int)g_ChumbyCmd;
  CHUMBY_DMA->PDC_RCR = CHUMBY_CMD_SIZE;
  
  g_ChumbyState = CHUMBY_STATE_RXT_CMD;
  g_ChumbyCmd[0] = 0;
  g_ChumbyCmd[1] = 0;
  g_ChumbyCmd[2] = 0 ;
  
  CHUMBY_SPI->SPI_IDR = AT91C_SPI_ENDTX;
  CHUMBY_SPI->SPI_IER = AT91C_SPI_ENDRX;
  ChumbyBend(0);
}

static void ChumbySetStateData(ushort count)
{
  CHUMBY_DMA->PDC_RPR = (unsigned int)g_ChumbyData;
  CHUMBY_DMA->PDC_RCR = count/*<<1*/;
  
  g_ChumbyState = CHUMBY_STATE_RXT_DATA;
  ChumbyBend(1);
}

static void ChumbySetStateWriteLen(ushort count)
{
  if(count < 1) {
    count = 1;
    g_ChumbyData[1] = 0;
  }

  g_ChumbyData[0] = count;
  CHUMBY_DMA->PDC_RPR = 0;
  
  CHUMBY_DMA->PDC_TPR = (unsigned int)g_ChumbyData;
  CHUMBY_DMA->PDC_TCR = 2;
  
  g_ChumbyState = CHUMBY_STATE_TXT_LEN;
  
  CHUMBY_SPI->SPI_IDR = AT91C_SPI_ENDRX;
  CHUMBY_SPI->SPI_IER = AT91C_SPI_ENDTX;
  ChumbyBend(0);
}

static void ChumbySetStateWriteData()
{
  CHUMBY_DMA->PDC_TPR = &(g_ChumbyData[1]);
  CHUMBY_DMA->PDC_TCR = g_ChumbyData[0]/*<<1*/;
  
  g_ChumbyState = CHUMBY_STATE_TXT_DATA;
  ChumbyBend(1);
}

static void ChumbyHandleCmd(ushort cmd, ushort *data, ushort length)
{
  int outcount = 0;
  switch(cmd) {
    case CBOB_CMD_READ_DIGITALS:
      g_ChumbyData[1] = 0x0A0A;
      outcount = 1;
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
