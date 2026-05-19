#include <string.h>

#include "LiotRfCmd.h"
#include "LiotUart.h"
#include "esp_log.h"
#include "microrl.h"

static const char *TAG = "main";

static microrl_t rl;
static microrl_t *prl = &rl;

static void microrlPrint(const char *str)
{
    LiotUartTx((uint8_t *)str, strlen(str));
}

static int UartRxCallback(uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        microrl_insert_char(prl, data[i]);
    }
    return 0;
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP LR2021 LoRa Example Started");

    LiotRfCmdInit();
    LiotRfCmdSetPrintfCallback(LiotUartTx);

    LiotUartInit();
    LiotUartSetRxCallback(UartRxCallback);

    microrl_init(prl, microrlPrint);
    microrl_set_execute_callback(prl, LiotRfCmdExe);
    microrl_set_complete_callback(prl, LiotRfCmdComplet);
    microrl_set_sigint_callback(prl, LiotRfCmdSigint);
}
