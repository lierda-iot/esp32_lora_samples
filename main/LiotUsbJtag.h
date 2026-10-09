#ifndef __LIOT_USB_JTAG_H__
#define __LIOT_USB_JTAG_H__

#include <stddef.h>
#include <stdint.h>

/* CLI transport over the ESP32-S3 built-in USB-Serial-JTAG */
typedef int (*LiotUsbJtagRxCallback)(uint8_t *data, size_t length);

void LiotUsbJtagInit(void);
int LiotUsbJtagTx(uint8_t *data, size_t length);
void LiotUsbJtagSetRxCallback(LiotUsbJtagRxCallback callback);

#endif
