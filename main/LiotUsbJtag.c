#include "LiotUsbJtag.h"

#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LiotUsbJtag";

#define USJ_RX_BUF_SIZE (4 * 1024)
#define USJ_TX_BUF_SIZE (4 * 1024)
#define USJ_PACK_LENGTH (2 * 1024) /* Buffer for a single read */

static uint8_t sUsjRxCache[USJ_PACK_LENGTH] = {0};
static LiotUsbJtagRxCallback sRxCallback    = NULL;

void LiotUsbJtagRxTask(void *argv)
{
    ESP_LOGI(TAG, "usb-jtag Rx task started");

    while (1)
    {
        /* Blocking read with a 100 ms timeout, any received data is passed to the callback */
        int len = usb_serial_jtag_read_bytes(sUsjRxCache, sizeof(sUsjRxCache), pdMS_TO_TICKS(100));
        if (len > 0 && sRxCallback)
        {
            sRxCallback(sUsjRxCache, len);
        }
    }

    vTaskDelete(NULL);
}

void LiotUsbJtagInit(void)
{
    ESP_LOGI(TAG, "usb-serial-jtag initialization");

    usb_serial_jtag_driver_config_t cfg = {
        .rx_buffer_size = USJ_RX_BUF_SIZE,
        .tx_buffer_size = USJ_TX_BUF_SIZE,
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));

    xTaskCreate(LiotUsbJtagRxTask, "Lierda_USJ_Rx_task", 10 * 1024, NULL, 4, NULL);
}

int LiotUsbJtagTx(uint8_t *data, size_t length)
{
    /* Blocking write into the USB-Serial-JTAG TX buffer, 100 ms timeout */
    int written = usb_serial_jtag_write_bytes((const char *)data, length, pdMS_TO_TICKS(100));
    if (written < 0)
    {
        ESP_LOGE(TAG, "usb-jtag write failed");
        return -1;
    }
    return 0;
}

void LiotUsbJtagSetRxCallback(LiotUsbJtagRxCallback callback)
{
    sRxCallback = callback;
}
