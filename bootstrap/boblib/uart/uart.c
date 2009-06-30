#include <bob.h>
#include <board.h>
#include <usart/usart.h>
#include <string.h>
#include "uart.h"

#include <usb/device/cdc-serial/CDCDSerialDriver.h>
#include <usb/device/cdc-serial/CDCDSerialDriverDescriptors.h>

#define USB_BUFFER_SIZE \
        BOARD_USB_ENDPOINTS_MAXPACKETSIZE(CDCDSerialDriverDescriptors_DATAIN)


char g_Uart0Buffer[UART_BUFFER_SIZE];
char g_Uart1Buffer[UART_BUFFER_SIZE];

char g_UartUsbReadBuffer[USB_BUFFER_SIZE];
char g_UartUsbWriteBuffer[USB_BUFFER_SIZE];
int  g_UartUsbWriteBufferBusy = 0;

int g_Uart0WriteIndex = 0;
int g_Uart0ReadIndex = 0;

int g_Uart1ReadIndex = 0;

static void UartUsbRead(void *pArg, 
												unsigned char status, 
												unsigned int received, 
												unsigned int remaining);

static void UartUsbWrite(void *pArg,
												 unsigned char status,
												 unsigned int transferred,
												 unsigned int remaining);
static int Uart0BytesAvailable();
static int Uart1BytesAvailable();

int UartInit()
{
	CDCDSerialDriver_Initialize();
  USBD_Connect();

	CDCDSerialDriver_Read(g_UartUsbReadBuffer, USB_BUFFER_SIZE, UartUsbRead, 0);
	
	Uart1BytesAvailable();
	return 1;
}

int UartWrite(int uart, char *data, int len)
{
	if(uart == 0) {
		if(g_UartUsbWriteBufferBusy)
			return 0;
		if(len > USB_BUFFER_SIZE)
			len = USB_BUFFER_SIZE;
		memcpy(g_UartUsbWriteBuffer, data, len);
		CDCDSerialDriver_Write(g_UartUsbWriteBuffer, len, UartUsbWrite, 0);
		return len;
	}
	else {
		return 0;
	}
}

int UartRead(int uart, char *data, int len)
{
	int copied = 0;
	if(uart == 0) {
		if(Uart0BytesAvailable() < len)
			len = Uart0BytesAvailable();
		if(len + g_Uart0ReadIndex >= UART_BUFFER_SIZE) {
			copied = UART_BUFFER_SIZE - g_Uart0ReadIndex;
			memcpy(data, g_Uart0Buffer, copied);
			g_Uart0ReadIndex = 0;
		}
		memcpy(data, g_Uart0Buffer, len - copied);
		g_Uart0ReadIndex += len - copied;
		return len;
	}
	else {
		return 0;
	}
}

int UartReset(int uart)
{
	if(uart == 0) {
		g_Uart0WriteIndex = 0;
		g_Uart0ReadIndex = 0;
		return 1;
	}
	else {
		return 0;
	}
}

static void UartUsbRead(void *pArg, 
												unsigned char status, 
												unsigned int received, 
												unsigned int remaining)
{
	int copied = 0;
	
	if(received + g_Uart0WriteIndex >= UART_BUFFER_SIZE) {
		copied = UART_BUFFER_SIZE - g_Uart0WriteIndex;
		memcpy(&(g_Uart0Buffer[g_Uart0WriteIndex]), g_UartUsbReadBuffer, copied);
		g_Uart0WriteIndex = 0;
	}
	
	memcpy(&(g_Uart0Buffer[g_Uart0WriteIndex]), &(g_UartUsbReadBuffer[copied]), received - copied);
	g_Uart0WriteIndex += received - copied;
	
	CDCDSerialDriver_Read(g_UartUsbReadBuffer, USB_BUFFER_SIZE, UartUsbRead, 0);
}

static void UartUsbWrite(void *pArg,
												 unsigned char status,
												 unsigned int transferred,
												 unsigned int remaining)
{
	g_UartUsbWriteBufferBusy = 0;
}

static int Uart0BytesAvailable()
{
	if(g_Uart0ReadIndex > g_Uart0WriteIndex)
		return UART_BUFFER_SIZE-g_Uart0ReadIndex + g_Uart0WriteIndex;
	return g_Uart0WriteIndex - g_Uart0ReadIndex;
}

static int Uart1BytesAvailable()
{
	return 0;
}
