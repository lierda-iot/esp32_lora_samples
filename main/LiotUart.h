#ifndef __LIOT_UART_H__
#define __LIOT_UART_H__

#include <stddef.h>
#include <stdint.h>

typedef int (*LiotUartRxCallback)(uint8_t *data, size_t length);

void LiotUartInit(void);
int LiotUartTx(uint8_t *data, size_t length);
void LiotUartSetRxCallback(LiotUartRxCallback callback);

#endif