#include <string.h>

#include "LiotRfCmd.h"
#include "LiotUsbJtag.h"
#include "esp_log.h"
#include "microrl.h"

static const char *TAG = "main";

static microrl_t rl;
static microrl_t *prl = &rl;

static void microrlPrint(const char *str)
{
    LiotUsbJtagTx((uint8_t *)str, strlen(str));
}

static int CliRxCallback(uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        microrl_insert_char(prl, data[i]);
    }
    return 0;
}

void app_main(void)
{
    /* The CLI shares the USB-Serial-JTAG port with the IDF console, keep IDF logs quiet */
    esp_log_level_set("*", ESP_LOG_ERROR);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "ESP LR2021 LoRa Example Started");

    LiotRfCmdInit();
    LiotRfCmdSetPrintfCallback(LiotUsbJtagTx);

    LiotUsbJtagInit();
    LiotUsbJtagSetRxCallback(CliRxCallback);

    microrl_init(prl, microrlPrint);
    microrl_set_execute_callback(prl, LiotRfCmdExe);
    microrl_set_complete_callback(prl, LiotRfCmdComplet);
    microrl_set_sigint_callback(prl, LiotRfCmdSigint);
}
