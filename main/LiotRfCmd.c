#include "LiotRfCmd.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "microrl_config.h"
#include "LiotLr2021.h"

static const char *TAG = "LiotRfCmd";

#define CLI_PRINT_BUF_SIZE 1024

#define CMD_ENDL ENDL

PrintfCallback gPrintfCallback = NULL;

/* Command line print function */
void LiotCmdPrint(const char *fmt, ...)
{
    if (gPrintfCallback == NULL)
    {
        return;
    }

    char buf[CLI_PRINT_BUF_SIZE];

    va_list args;
    va_start(args, fmt);

    vsnprintf(buf, sizeof(buf), fmt, args);

    va_end(args);

    gPrintfCallback((uint8_t *)buf, strlen(buf));
}

#if defined(CONFIG_SMTC_RADIO_SX126X)
ralf_t smtc_rac_radio = RALF_SX126X_INSTANTIATE(NULL);
#elif defined(CONFIG_SMTC_RADIO_LR11XX)
ralf_t smtc_rac_radio = RALF_LR11XX_INSTANTIATE(NULL);
#elif defined(CONFIG_SMTC_RADIO_LR20XX)
ralf_t smtc_rac_radio = RALF_LR20XX_INSTANTIATE(NULL);
#else
#error "Please select radio board.."
#endif

ral_pkt_type_t sCurrentModem = RAL_PKT_TYPE_LORA;

#ifndef RF_FREQ_IN_HZ
#define RF_FREQ_IN_HZ 868000000
#endif

#ifndef TX_OUTPUT_POWER_DBM
#define TX_OUTPUT_POWER_DBM 14
#endif

#ifndef LORA_SYNCWORD
#define LORA_SYNCWORD LORA_PRIVATE_NETWORK_SYNCWORD
#endif

#ifndef LORA_SF
#define LORA_SF RAL_LORA_SF7
#endif

#ifndef LORA_BW
#define LORA_BW RAL_LORA_BW_125_KHZ
#endif

#ifndef LORA_CR
#define LORA_CR RAL_LORA_CR_4_5
#endif

#ifndef LORA_PREAMBLE_LEN
#define LORA_PREAMBLE_LEN 8
#endif

#ifndef LORA_HEADER_TYPE
#define LORA_HEADER_TYPE RAL_LORA_PKT_EXPLICIT
#endif

#ifndef LORA_CRC
#define LORA_CRC true
#endif

static ralf_params_lora_t sRalfParamLora = {
    .rf_freq_in_hz     = RF_FREQ_IN_HZ,
    .output_pwr_in_dbm = TX_OUTPUT_POWER_DBM,
    .sync_word         = LORA_SYNCWORD,

    .mod_params =
        {
            .sf   = LORA_SF,
            .bw   = LORA_BW,
            .cr   = LORA_CR,
            .ldro = 0,
        },
    .pkt_params =
        {
            .preamble_len_in_symb = LORA_PREAMBLE_LEN,
            .header_type          = LORA_HEADER_TYPE,
            // .pld_len_in_bytes     = 16,
            .crc_is_on       = LORA_CRC,
            .invert_iq_is_on = false,
        },

};

static ralf_params_flrc_t sRalfParamFlrc = {0};

typedef struct
{
    uint32_t receivePkt;
    uint32_t successPkt;
    int16_t signalRssi;
    int8_t snr;
    uint32_t crcErr;
    uint32_t headerErr;

    bool enable;

} per_stat_t;

static per_stat_t gPerStat = {0};
static void per_print(void);

/* LoRa sleep status */
static uint8_t gLoraSleepStatus = false;

static uint32_t sRxTimeoutAfterCad = 0;

#define EVT_RADIO_GPIO_INT BIT0

static EventGroupHandle_t sRadioEventGroup;

void RadioIrqCallback(void *context)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(sRadioEventGroup, EVT_RADIO_GPIO_INT, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

static void LiotRfCmdRfResetAndInit(void)
{
    if (ral_reset(&(smtc_rac_radio.ral)) != RAL_STATUS_OK)
    {
        ESP_LOGE(TAG, "RAL reset failed!");
        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
    }
    ESP_LOGD(TAG, "RAL reset completed successfully");

    if (ral_init(&(smtc_rac_radio.ral)) != RAL_STATUS_OK)
    {
        ESP_LOGE(TAG, "RAL initialization failed!");
        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
    }
    ESP_LOGD(TAG, "RAL initialization completed successfully");

    if (ral_clear_irq_status(&(smtc_rac_radio.ral), RAL_IRQ_ALL) != RAL_STATUS_OK)
    {
        ESP_LOGE(TAG, "Failed to clear irq!");
        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
    }
    ESP_LOGD(TAG, "RAL clear irq successfully");

    switch (sCurrentModem)
    {
        case RAL_PKT_TYPE_LORA:
            if (ralf_setup_lora(&(smtc_rac_radio), &sRalfParamLora) != RAL_STATUS_OK)
            {
                ESP_LOGE(TAG, "Failed to setup lora!");
                SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
            }
            ESP_LOGD(TAG, "RALF setup lora successfully");
            break;

        case RAL_PKT_TYPE_GFSK:

            break;

        case RAL_PKT_TYPE_FLRC:

            break;

        default:
            break;
    }

    if (ral_set_dio_irq_params(&(smtc_rac_radio.ral),
                               RAL_IRQ_TX_DONE | RAL_IRQ_RX_DONE | RAL_IRQ_RX_TIMEOUT | RAL_IRQ_RX_HDR_ERROR |
                                   RAL_IRQ_RX_CRC_ERROR | RAL_IRQ_CAD_OK | RAL_IRQ_CAD_DONE) != RAL_STATUS_OK)
    {
        ESP_LOGE(TAG, "Failed to set dio irq!");
        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
    }
    ESP_LOGD(TAG, "RAL set dio irq successfully");

    if (ral_set_rx_tx_fallback_mode(&(smtc_rac_radio.ral), RAL_FALLBACK_STDBY_XOSC) != RAL_STATUS_OK)
    {
        ESP_LOGE(TAG, "Failed to set rx tx fallback mode!");
        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
    }
    ESP_LOGD(TAG, "RAL set rx tx fallback mode successfully");

    if (ral_set_standby(&(smtc_rac_radio.ral), RAL_STANDBY_CFG_XOSC) != RAL_STATUS_OK)
    {
        ESP_LOGE(TAG, "Failed to set standby!");
        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
    }
    ESP_LOGD(TAG, "RAL set standby successfully");

    smtc_modem_hal_irq_config_radio_irq(RadioIrqCallback, NULL);
    ESP_LOGD(TAG, "Radio IRQ callback configured");
}

#define RADIO_FIFO_LENGTH 256

void LiotRadioIrqTask(void *argv)
{
    uint16_t receivedPackSize = 0;
    uint8_t receivedPackBuff[RADIO_FIFO_LENGTH];
    ral_irq_t radioIrq = 0;

    ESP_LOGI(TAG, "LR2021 task started");

    LiotRfCmdRfResetAndInit();

    while (1)
    {
        EventBits_t bits = xEventGroupWaitBits(sRadioEventGroup,
                                               EVT_RADIO_GPIO_INT, /* 等待的事件位 */
                                               pdTRUE,             /* 退出时清除事件位 */
                                               pdFALSE,            /* 等待任意一个 bit */
                                               portMAX_DELAY       /* 永久阻塞 */
        );

        if (bits & EVT_RADIO_GPIO_INT)
        {
            ESP_LOGD(TAG, "radio interrupt event received");
            if (ral_get_and_clear_irq_status(&(smtc_rac_radio.ral), &radioIrq) != RAL_STATUS_OK)
            {
                SMTC_MODEM_HAL_PANIC();
            }
            if ((radioIrq & RAL_IRQ_TX_DONE) == RAL_IRQ_TX_DONE)
            {
                LiotCmdPrint("TX done!\r\n");
            }
            else if ((radioIrq & RAL_IRQ_RX_DONE) == RAL_IRQ_RX_DONE)
            {
                ESP_LOGD(TAG, "RX done irq received");

                ral_get_pkt_payload(&(smtc_rac_radio.ral), RADIO_FIFO_LENGTH, receivedPackBuff, &receivedPackSize);
                ESP_LOGD(TAG, "RAL get payload %d", receivedPackSize);

                /* 接着开启 RX 且清除 fifo*/
                if (ral_set_rx(&(smtc_rac_radio.ral), 0) != RAL_STATUS_OK)
                {
                    ESP_LOGE(TAG, "Failed to set rx!");
                    SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
                }
                ESP_LOGD(TAG, "RAL set rx successfully");

                LiotCmdPrint("\r\n RX done, size: %d, data: ", receivedPackSize);
                for (int i = 0; i < receivedPackSize; i++)
                {
                    LiotCmdPrint("%02X ", receivedPackBuff[i]);
                }
                LiotCmdPrint("\r\n");

                if ((radioIrq & RAL_IRQ_RX_CRC_ERROR) == RAL_IRQ_RX_CRC_ERROR)
                {
                    ESP_LOGE(TAG, "RX CRC error irq received");

                    ral_lora_rx_pkt_status_t lora_rx_pkt_status;
                    ral_get_lora_rx_pkt_status(&(smtc_rac_radio.ral), &lora_rx_pkt_status);

                    gPerStat.receivePkt++;
                    gPerStat.signalRssi = lora_rx_pkt_status.signal_rssi_pkt_in_dbm;
                    gPerStat.snr        = lora_rx_pkt_status.snr_pkt_in_db;
                    gPerStat.crcErr++;

                    per_print();
                }
                else
                {
                    ral_lora_rx_pkt_status_t lora_rx_pkt_status;
                    ral_get_lora_rx_pkt_status(&(smtc_rac_radio.ral), &lora_rx_pkt_status);

                    gPerStat.receivePkt++;
                    gPerStat.successPkt++;
                    gPerStat.signalRssi = lora_rx_pkt_status.signal_rssi_pkt_in_dbm;
                    gPerStat.snr        = lora_rx_pkt_status.snr_pkt_in_db;

                    per_print();
                }
            }
            else if ((radioIrq & RAL_IRQ_RX_TIMEOUT) == RAL_IRQ_RX_TIMEOUT)
            {
                ESP_LOGW(TAG, "RX timeout irq received");
            }
            else if ((radioIrq & RAL_IRQ_RX_HDR_ERROR) == RAL_IRQ_RX_HDR_ERROR)
            {
                ESP_LOGE(TAG, "RX header error irq received");

                ral_lora_rx_pkt_status_t lora_rx_pkt_status;
                ral_get_lora_rx_pkt_status(&(smtc_rac_radio.ral), &lora_rx_pkt_status);

                gPerStat.receivePkt++;
                gPerStat.signalRssi = lora_rx_pkt_status.signal_rssi_pkt_in_dbm;
                gPerStat.snr        = lora_rx_pkt_status.snr_pkt_in_db;
                gPerStat.headerErr++;

                per_print();
            }
            else if ((radioIrq & RAL_IRQ_CAD_OK) == RAL_IRQ_CAD_OK)
            {
                LiotCmdPrint("CAD Positive (Preamble Detected)!" CMD_ENDL);
                /* CAD 监测到活动，开启 RX */
                if (ral_set_rx(&(smtc_rac_radio.ral), sRxTimeoutAfterCad) != RAL_STATUS_OK)
                {
                    ESP_LOGE(TAG, "Failed to set rx after CAD!");
                }
                else
                {
                    ESP_LOGD(TAG, "RAL set rx after CAD successfully, timeout: %lu ms", sRxTimeoutAfterCad);
                }
            }
            else if ((radioIrq & RAL_IRQ_CAD_DONE) == RAL_IRQ_CAD_DONE)
            {
                LiotCmdPrint("CAD Done (No activity detected)." CMD_ENDL);
            }
        }
    }
    vTaskDelete(NULL);
}

#define VERSION "1.0.0"
#define PRODUCT "ESP LR2021 LoRa Example"

struct cli_cmd;
typedef int (*cli_func_t)(const struct cli_cmd *cmd, int argc, const char *const *argv);

typedef struct cli_cmd
{
    const char *name;
    cli_func_t func;
    /* Sub-commands */
    const struct cli_cmd *sub;
    uint16_t sub_num;

    const char *usage;
    const char *desc;
} cli_cmd_t;

static int cmd_help(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_clear(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_list(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_info(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_show(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_modem(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_syncword(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_freq(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_power(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_tx(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_rx(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_standby(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_sleep(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_wakeup(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_per(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_reset(const cli_cmd_t *cmd, int argc, const char *const *argv);

static int cmd_rf_lora_sf(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_lora_bw(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_lora_cr(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_lora_preamble(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_lora_syncword(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_lora_cad(const cli_cmd_t *cmd, int argc, const char *const *argv);

static int cmd_rf_tx_cw(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_tx_preamble(const cli_cmd_t *cmd, int argc, const char *const *argv);

static const cli_cmd_t cmd_table[] = {
    {"help", cmd_help, NULL, 0, NULL, "Show command list and usage"},
    {"clear", cmd_clear, NULL, 0, NULL, "Clear terminal screen"},
    {"list", cmd_list, NULL, 0, NULL, "List items"},
    {"info", cmd_info, NULL, 0, NULL, "Show product and firmware information"},

    {"show", cmd_rf_show, NULL, 0, NULL, "Show RF configuration"},
    {"modem", cmd_rf_modem, NULL, 0, "modem <lora|gfsk|flrc>", "Get/Set RF modem type"},
    {"syncword", cmd_rf_syncword, NULL, 0, "syncword <hex_value>", "Get/Set RF sync word"},
    {"freq", cmd_rf_freq, NULL, 0, "freq <frequency_in_hz>", "Get/Set RF frequency"},
    {"power", cmd_rf_power, NULL, 0, "power <dBm>", "Get/Set RF TX power"},
    {"tx", cmd_rf_tx, NULL, 0, "tx <hex byte> [hex byte] ...", "Send RF packet"},
    {"rx", cmd_rf_rx, NULL, 0, NULL, "Start RF receive"},
    {"standby", cmd_rf_standby, NULL, 0, NULL, "Put RF in standby mode"},
    {"sleep", cmd_rf_sleep, NULL, 0, "sleep <0|1>", "Put RF in sleep mode (1=retain config on wakeup)"},
    {"wakeup", cmd_rf_wakeup, NULL, 0, NULL, "Wake up RF from sleep mode"},
    {"per", cmd_rf_per, NULL, 0, "per <0|1>", "Enable/disable PER measurement"},
    {"reset", cmd_rf_reset, NULL, 0, NULL, "Reset radio"},

    {"lora_sf", cmd_rf_lora_sf, NULL, 0, "lora_sf <5~12>", "Get/Set LoRa spreading factor"},
    {"lora_bw", cmd_rf_lora_bw, NULL, 0, "lora_bw <31|62|125|250|500|1000>", "Get/Set LoRa bandwidth"},
    {"lora_cr", cmd_rf_lora_cr, NULL, 0, "lora_cr <4/5|4/6|4/7|4/8|li4/5|li4/6|li4/8>", "Get/Set LoRa coding rate"},
    {"lora_preamble", cmd_rf_lora_preamble, NULL, 0, "lora_preamble <length>", "Get/Set LoRa preamble length"},
    {"lora_syncword", cmd_rf_lora_syncword, NULL, 0, "lora_syncword <0x12|0x34>", "Get/Set LoRa sync word"},
    {"lora_cad", cmd_rf_lora_cad, NULL, 0, "lora_cad [rx_timeout_ms]", "Start LoRa CAD detect"},

    {"tx_cw", cmd_rf_tx_cw, NULL, 0, NULL, "Transmit CW signal"},
    {"tx_preamble", cmd_rf_tx_preamble, NULL, 0, NULL, "Transmit infinite preamble"},
};

#define CMD_COUNT (sizeof(cmd_table) / sizeof(cmd_table[0]))

/***************************  help  ****************************/
static int cmd_help(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    for (int i = 0; i < CMD_COUNT; i++)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd_table[i].name);
        if (cmd_table[i].usage)
        {
            LiotCmdPrint("  %s" CMD_ENDL, cmd_table[i].usage);
        }
        if (cmd_table[i].desc)
        {
            LiotCmdPrint("  %s" CMD_ENDL, cmd_table[i].desc);
        }
        LiotCmdPrint(CMD_ENDL);
    }
    return 0;
}

/***************************  clear  ****************************/
static int cmd_clear(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    LiotCmdPrint("\033[2J"); // ESC seq for clear entire screen
    LiotCmdPrint("\033[H");  // ESC seq for move cursor at left-top corner
    return 0;
}

/***************************  list  ****************************/
static int cmd_list(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    for (int i = 0; i < CMD_COUNT; i++)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd_table[i].name);
    }
    return 0;
}

/***************************  info  ****************************/
static int cmd_info(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    LiotCmdPrint(CMD_ENDL "Product  : %s" CMD_ENDL, PRODUCT);
    LiotCmdPrint("FW Ver   : %s" CMD_ENDL, VERSION);
    LiotCmdPrint("Build    : %s %s" CMD_ENDL, __DATE__, __TIME__);
    return 0;
}

/***************************  RF  ****************************/
static int cmd_rf_show(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    LiotCmdPrint(CMD_ENDL "LR2021 RF Configuration" CMD_ENDL);
    LiotCmdPrint(CMD_ENDL "---------------------- " CMD_ENDL);
    LiotCmdPrint(CMD_ENDL "modem       : %s " CMD_ENDL,
                 sCurrentModem == RAL_PKT_TYPE_LORA   ? "lora"
                 : sCurrentModem == RAL_PKT_TYPE_GFSK ? "gfsk"
                                                      : "flrc");
    switch (sCurrentModem)
    {
        case RAL_PKT_TYPE_LORA:
            LiotCmdPrint("sync word   : 0x%02X" CMD_ENDL, sRalfParamLora.sync_word);
            LiotCmdPrint("frequency   : %u Hz" CMD_ENDL, sRalfParamLora.rf_freq_in_hz);
            break;
        case RAL_PKT_TYPE_GFSK:

            break;
        case RAL_PKT_TYPE_FLRC:

            break;
        default:
            LiotCmdPrint("Invalid modem type" CMD_ENDL);
            break;
    }
    return 0;
}

static int cmd_rf_modem(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc != 1 && argc != 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        LiotCmdPrint("Current modem: %s" CMD_ENDL,
                     sCurrentModem == RAL_PKT_TYPE_LORA   ? "lora"
                     : sCurrentModem == RAL_PKT_TYPE_GFSK ? "gfsk"
                                                          : "flrc");
        return 0;
    }
    else
    {
        if (strcmp(argv[1], "lora") == 0)
        {
            sCurrentModem = RAL_PKT_TYPE_LORA;
        }
        else if (strcmp(argv[1], "gfsk") == 0)
        {
            sCurrentModem = RAL_PKT_TYPE_GFSK;
        }
        else if (strcmp(argv[1], "flrc") == 0)
        {
            sCurrentModem = RAL_PKT_TYPE_FLRC;
        }
        else
        {
            LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        }
    }

    return 0;
}

static int cmd_rf_syncword(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc == 1)
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:
                LiotCmdPrint("Current lora sync word: 0x%02X" CMD_ENDL, sRalfParamLora.sync_word);
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                break;
        }
    }
    else
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:

                char *endptr;
                uint8_t sync_word = (uint8_t)strtoul(argv[1], &endptr, 16);
                if (*endptr != '\0')
                {
                    LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
                    return -1;
                }

                if (sync_word != LORA_PRIVATE_NETWORK_SYNCWORD && sync_word != LORA_PUBLIC_NETWORK_SYNCWORD)
                {
                    LiotCmdPrint("Invalid sync word for LORA, must be %d or %d" CMD_ENDL,
                                 LORA_PRIVATE_NETWORK_SYNCWORD,
                                 LORA_PUBLIC_NETWORK_SYNCWORD);
                    return -1;
                }

                if (sync_word != sRalfParamLora.sync_word)
                {
                    sRalfParamLora.sync_word = sync_word;
                    if (ralf_setup_lora(&(smtc_rac_radio), &sRalfParamLora) != RAL_STATUS_OK)
                    {
                        ESP_LOGE(TAG, "Failed to setup lora!");
                        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
                    }
                    ESP_LOGD(TAG, "setup lora sync word successfully");
                }

                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                return -1;
        }
    }

    return 0;
}

static int cmd_rf_freq(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc != 1 && argc != 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:
                LiotCmdPrint("Current lora frequency: %u Hz" CMD_ENDL, sRalfParamLora.rf_freq_in_hz);
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                break;
        }
    }
    else
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:

                char *endptr;
                uint32_t rf_freq_in_hz = (uint32_t)strtoul(argv[1], &endptr, 10);
                if (*endptr != '\0')
                {
                    LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
                    return -1;
                }


                if (rf_freq_in_hz != sRalfParamLora.rf_freq_in_hz)
                {
                    sRalfParamLora.rf_freq_in_hz = rf_freq_in_hz;
                    if (ral_set_rf_freq(&(smtc_rac_radio.ral), sRalfParamLora.rf_freq_in_hz) != RAL_STATUS_OK)
                    {
                        ESP_LOGE(TAG, "Failed to setup lora!");
                        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
                    }
                    ESP_LOGD(TAG, "setup lora freq successfully");
                }
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                return -1;
        }
    }

    return 0;
}

static int cmd_rf_power(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc != 1 && argc != 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:
                LiotCmdPrint("Current lora power: %u dBm" CMD_ENDL, sRalfParamLora.output_pwr_in_dbm);
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                break;
        }
    }
    else
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:

                char *endptr;
                uint32_t output_pwr_in_dbm = (uint32_t)strtoul(argv[1], &endptr, 10);
                if (*endptr != '\0')
                {
                    LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
                    return -1;
                }


                if (output_pwr_in_dbm != sRalfParamLora.output_pwr_in_dbm)
                {
                    sRalfParamLora.output_pwr_in_dbm = output_pwr_in_dbm;

                    if (ral_set_tx_cfg(&(smtc_rac_radio.ral),
                                       sRalfParamLora.output_pwr_in_dbm,
                                       sRalfParamLora.rf_freq_in_hz) != RAL_STATUS_OK)
                    {
                        ESP_LOGE(TAG, "Failed to setup lora!");
                        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
                    }
                    ESP_LOGD(TAG, "setup lora power successfully");
                }
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                return -1;
        }
    }

    return 0;
}

static int cmd_rf_tx(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc < 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    switch (sCurrentModem)
    {
        case RAL_PKT_TYPE_LORA:

            uint8_t payload[256];
            int len = argc - 1;

            if (len > sizeof(payload))
            {
                LiotCmdPrint("Payload too long" CMD_ENDL);
                return -1;
            }

            for (int i = 0; i < len; i++)
            {
                char *endptr;
                long val = strtol(argv[i + 1], &endptr, 16);

                if (*endptr != '\0' || val < 0 || val > 0xFF)
                {
                    LiotCmdPrint("Invalid hex byte: %s" CMD_ENDL, argv[i + 1]);
                    return -1;
                }

                payload[i] = (uint8_t)val;
            }

            LiotCmdPrint("TX %d bytes:" CMD_ENDL, len);
            for (int i = 0; i < len; i++)
            {
                LiotCmdPrint("%02X ", payload[i]);
            }
            LiotCmdPrint(CMD_ENDL);

            sRalfParamLora.pkt_params.pld_len_in_bytes = len;
            if (ralf_setup_lora(&(smtc_rac_radio), &sRalfParamLora) != RAL_STATUS_OK)
            {
                ESP_LOGE(TAG, "Failed to setup lora!");
                SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
            }
            ESP_LOGD(TAG, "RALF setup lora successfully");

            if (ral_set_pkt_payload(&(smtc_rac_radio.ral), payload, len) != RAL_STATUS_OK)
            {
                ESP_LOGE(TAG, "Failed to set pkt payload!");
                SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
            }
            ESP_LOGD(TAG, "RAL set pkt payload successfully");

            if (ral_set_tx(&(smtc_rac_radio.ral)) != RAL_STATUS_OK)
            {
                ESP_LOGE(TAG, "Failed to set tx!");
                SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
            }
            ESP_LOGD(TAG, "RAL set tx successfully");
            break;
        case RAL_PKT_TYPE_GFSK:

            break;
        case RAL_PKT_TYPE_FLRC:

            break;
        default:
            LiotCmdPrint("Invalid modem type" CMD_ENDL);
            return -1;
    }

    return 0;
}

static int cmd_rf_rx(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    switch (sCurrentModem)
    {
        case RAL_PKT_TYPE_LORA:
            sRalfParamLora.pkt_params.pld_len_in_bytes = 0;
            if (ralf_setup_lora(&(smtc_rac_radio), &sRalfParamLora) != RAL_STATUS_OK)
            {
                ESP_LOGE(TAG, "Failed to setup lora!");
                SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
            }
            ESP_LOGD(TAG, "RALF setup lora successfully");

            if (ral_set_rx(&(smtc_rac_radio.ral), 0) != RAL_STATUS_OK)
            {
                ESP_LOGE(TAG, "Failed to set rx!");
                SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
            }

            ESP_LOGD(TAG, "RAL set rx successfully");
            break;
        case RAL_PKT_TYPE_GFSK:

            break;
        case RAL_PKT_TYPE_FLRC:
            sRalfParamFlrc.pkt_params.pld_len_in_bytes = 256;
            if (ralf_setup_flrc(&(smtc_rac_radio), &sRalfParamFlrc) != RAL_STATUS_OK)
            {
                ESP_LOGE(TAG, "Failed to setup flrc!");
                SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
            }
            ESP_LOGD(TAG, "RALF setup flrc successfully");

            if (ral_set_rx(&(smtc_rac_radio.ral), 0) != RAL_STATUS_OK)
            {
                ESP_LOGE(TAG, "Failed to set rx!");
                SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
            }

            ESP_LOGD(TAG, "RAL set rx successfully");
            break;
        default:
            LiotCmdPrint("Invalid modem type" CMD_ENDL);
            return -1;
    }

    return 0;
}

static int cmd_rf_standby(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (ral_set_standby(&(smtc_rac_radio.ral), RAL_STANDBY_CFG_XOSC) != RAL_STATUS_OK)
    {
        ESP_LOGE(TAG, "Failed to set standby!");
        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
    }
    ESP_LOGD(TAG, "RAL set standby successfully");

    return 0;
}

static int cmd_rf_sleep(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc == 1)
    {
        LiotCmdPrint("Current lora sleep status: 0x%02X" CMD_ENDL, gLoraSleepStatus);
    }
    else
    {
        char *endptr;
        uint8_t retain_config = (uint8_t)strtoul(argv[1], &endptr, 16);
        if (*endptr != '\0')
        {
            LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
            return -1;
        }

        if (retain_config != true && retain_config != false)
        {
            LiotCmdPrint("Invalid retain config value,must be 0 or 1" CMD_ENDL);
            return -1;
        }

        if (ral_set_sleep(&(smtc_rac_radio.ral), retain_config) != RAL_STATUS_OK)
        {
            ESP_LOGE(TAG, "Failed to set sleep!");
            SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
        }
        ESP_LOGD(TAG, "RAL set sleep successfully");
        gLoraSleepStatus = true;
    }
    return 0;
}

static int cmd_rf_wakeup(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (ral_wakeup(&(smtc_rac_radio.ral)) != RAL_STATUS_OK)
    {
        ESP_LOGE(TAG, "Failed to wakeup!");
        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
    }
    ESP_LOGD(TAG, "RAL wakeup successfully");
    gLoraSleepStatus = false;

    return 0;
}

static void per_print(void)
{
    if (gPerStat.enable == true)
    {
        LiotCmdPrint("PER: packet num=%lu success num=%lu Signal RSSI=%d SNR=%d  crc err=%lu  header error=%lu\r\n",
                     gPerStat.receivePkt,
                     gPerStat.successPkt,
                     gPerStat.signalRssi,
                     gPerStat.snr,
                     gPerStat.crcErr,
                     gPerStat.headerErr);
    }
}

static int cmd_rf_per(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc != 1 && argc != 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        LiotCmdPrint("Current per %s" CMD_ENDL, gPerStat.enable ? "enabled" : "disabled");
    }
    else
    {
        char *endptr;
        uint32_t enable = (uint32_t)strtoul(argv[1], &endptr, 10);
        if (*endptr != '\0')
        {
            LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
            return -1;
        }

        if (enable != true && enable != false)
        {
            LiotCmdPrint("Invalid enable value,must be 0 or 1" CMD_ENDL);
            return -1;
        }

        if (enable == true)
        {
            memset(&gPerStat, 0, sizeof(gPerStat));
            LiotCmdPrint("PER enabled and reset" CMD_ENDL);
            gPerStat.enable = true;

            LiotCmdPrint("start RX" CMD_ENDL);
            switch (sCurrentModem)
            {
                case RAL_PKT_TYPE_LORA:
                    sRalfParamLora.pkt_params.pld_len_in_bytes = 0;
                    if (ralf_setup_lora(&(smtc_rac_radio), &sRalfParamLora) != RAL_STATUS_OK)
                    {
                        ESP_LOGE(TAG, "Failed to setup lora!");
                        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
                    }
                    ESP_LOGD(TAG, "RALF setup lora successfully");

                    if (ral_set_rx(&(smtc_rac_radio.ral), 0) != RAL_STATUS_OK)
                    {
                        ESP_LOGE(TAG, "Failed to set rx!");
                        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
                    }

                    ESP_LOGD(TAG, "RAL set rx successfully");
                    break;
                case RAL_PKT_TYPE_GFSK:

                    break;
                case RAL_PKT_TYPE_FLRC:
                    sRalfParamFlrc.pkt_params.pld_len_in_bytes = 256;
                    if (ralf_setup_flrc(&(smtc_rac_radio), &sRalfParamFlrc) != RAL_STATUS_OK)
                    {
                        ESP_LOGE(TAG, "Failed to setup flrc!");
                        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
                    }
                    ESP_LOGD(TAG, "RALF setup flrc successfully");

                    if (ral_set_rx(&(smtc_rac_radio.ral), 0) != RAL_STATUS_OK)
                    {
                        ESP_LOGE(TAG, "Failed to set rx!");
                        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
                    }

                    ESP_LOGD(TAG, "RAL set rx successfully");
                    break;
                default:
                    LiotCmdPrint("Invalid modem type" CMD_ENDL);
                    return -1;
            }
        }
        else
        {
            gPerStat.enable = false;
            if (ral_set_standby(&(smtc_rac_radio.ral), RAL_STANDBY_CFG_XOSC) != RAL_STATUS_OK)
            {
                ESP_LOGE(TAG, "Failed to set standby!");
                SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
            }
            ESP_LOGD(TAG, "RAL set standby successfully");

            LiotCmdPrint("PER disabled" CMD_ENDL);
        }
    }

    return 0;
}

static int cmd_rf_reset(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    LiotRfCmdRfResetAndInit();
    return 0;
}

static int cmd_rf_lora_sf(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc != 1 && argc != 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:
                LiotCmdPrint("Current lora sf: %u" CMD_ENDL, sRalfParamLora.mod_params.sf);
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                break;
        }
    }
    else
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:

                char *endptr;
                uint32_t sf = (uint32_t)strtoul(argv[1], &endptr, 10);
                if (*endptr != '\0')
                {
                    LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
                    return -1;
                }


                if (sf != sRalfParamLora.mod_params.sf)
                {
                    sRalfParamLora.mod_params.sf = sf;
                    if (ral_set_lora_mod_params(&(smtc_rac_radio.ral), &sRalfParamLora.mod_params) != RAL_STATUS_OK)
                    {
                        ESP_LOGE(TAG, "Failed to setup lora!");
                        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
                    }
                    ESP_LOGD(TAG, "setup lora sf successfully");
                }
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                return -1;
        }
    }

    return 0;
}

typedef struct
{
    const char *name;
    ral_lora_bw_t bw;
} lora_bw_map_t;

static const lora_bw_map_t lora_bw_table[] = {
    {"31", RAL_LORA_BW_031_KHZ},
    {"62", RAL_LORA_BW_062_KHZ},
    {"125", RAL_LORA_BW_125_KHZ},
    {"250", RAL_LORA_BW_250_KHZ},
    {"500", RAL_LORA_BW_500_KHZ},
    {"1000", RAL_LORA_BW_1000_KHZ},
};

static int parse_lora_bw(const char *str, ral_lora_bw_t *out)
{
    for (int i = 0; i < sizeof(lora_bw_table) / sizeof(lora_bw_table[0]); i++)
    {
        if (strcmp(str, lora_bw_table[i].name) == 0)
        {
            *out = lora_bw_table[i].bw;
            return 0;
        }
    }

    return -1;
}

static int cmd_rf_lora_bw(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc != 1 && argc != 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:
                LiotCmdPrint("Current lora bw: %u" CMD_ENDL, sRalfParamLora.mod_params.bw);
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                break;
        }
    }
    else
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:

                ral_lora_bw_t bw;
                if (parse_lora_bw(argv[1], &bw) != 0)
                {
                    LiotCmdPrint("Invalid lora bw: %s" CMD_ENDL, argv[1]);
                    return -1;
                }

                if (bw != sRalfParamLora.mod_params.bw)
                {
                    sRalfParamLora.mod_params.bw = bw;
                    if (ral_set_lora_mod_params(&(smtc_rac_radio.ral), &sRalfParamLora.mod_params) != RAL_STATUS_OK)
                    {
                        ESP_LOGE(TAG, "Failed to setup lora!");
                        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
                    }
                    ESP_LOGD(TAG, "setup lora bw successfully");
                }
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                return -1;
        }
    }

    return 0;
}

typedef struct
{
    const char *name;
    ral_lora_cr_t cr;
} lora_cr_map_t;

static const lora_cr_map_t lora_cr_table[] = {
    {"4/5", RAL_LORA_CR_4_5},
    {"4/6", RAL_LORA_CR_4_6},
    {"4/7", RAL_LORA_CR_4_7},
    {"4/8", RAL_LORA_CR_4_8},
    {"li4/5", RAL_LORA_CR_LI_4_5},
    {"li4/6", RAL_LORA_CR_LI_4_6},
    {"li4/8", RAL_LORA_CR_LI_4_8},
};

static int parse_lora_cr(const char *str, ral_lora_cr_t *out)
{
    for (int i = 0; i < sizeof(lora_cr_table) / sizeof(lora_cr_table[0]); i++)
    {
        if (strcmp(str, lora_cr_table[i].name) == 0)
        {
            *out = lora_cr_table[i].cr;
            return 0;
        }
    }

    return -1;
}

static int cmd_rf_lora_cr(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc != 1 && argc != 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:
                LiotCmdPrint("Current lora cr: %u" CMD_ENDL, sRalfParamLora.mod_params.cr);
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                break;
        }
    }
    else
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:
                ral_lora_cr_t cr;
                if (parse_lora_cr(argv[1], &cr) != 0)
                {
                    LiotCmdPrint("Invalid lora cr: %s" CMD_ENDL, argv[1]);
                    return -1;
                }

                if (cr != sRalfParamLora.mod_params.cr)
                {
                    sRalfParamLora.mod_params.cr = cr;
                    if (ral_set_lora_mod_params(&(smtc_rac_radio.ral), &sRalfParamLora.mod_params) != RAL_STATUS_OK)
                    {
                        ESP_LOGE(TAG, "Failed to setup lora!");
                        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
                    }
                    ESP_LOGD(TAG, "setup lora cr successfully");
                }
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                return -1;
        }
    }

    return 0;
}

static int cmd_rf_lora_preamble(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc != 1 && argc != 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:
                LiotCmdPrint("Current lora preamble: %u" CMD_ENDL, sRalfParamLora.pkt_params.preamble_len_in_symb);
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                break;
        }
    }
    else
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:
                char *endptr;
                uint32_t preamble = (uint32_t)strtoul(argv[1], &endptr, 10);
                if (*endptr != '\0')
                {
                    LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
                    return -1;
                }

                if (preamble != sRalfParamLora.pkt_params.preamble_len_in_symb)
                {
                    sRalfParamLora.pkt_params.preamble_len_in_symb = preamble;
                    if (ral_set_lora_pkt_params(&(smtc_rac_radio.ral), &sRalfParamLora.pkt_params) != RAL_STATUS_OK)
                    {
                        ESP_LOGE(TAG, "Failed to setup lora!");
                        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
                    }
                    ESP_LOGD(TAG, "setup lora preamble successfully");
                }
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                return -1;
        }
    }

    return 0;
}

static int cmd_rf_lora_syncword(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc != 1 && argc != 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:
                LiotCmdPrint("Current lora syncword: %02x" CMD_ENDL, sRalfParamLora.sync_word);
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                break;
        }
    }
    else
    {
        switch (sCurrentModem)
        {
            case RAL_PKT_TYPE_LORA:
                char *endptr;
                uint32_t syncword = (uint32_t)strtoul(argv[1], &endptr, 16);
                if (*endptr != '\0')
                {
                    LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
                    return -1;
                }

                if (syncword != 0x12 && syncword != 0x34)
                {
                    LiotCmdPrint("Invalid syncword. Please use 0x12 or 0x34." CMD_ENDL);
                    return -1;
                }

                if (syncword != sRalfParamLora.sync_word)
                {
                    sRalfParamLora.sync_word = syncword;
                    if (ral_set_lora_sync_word(&(smtc_rac_radio.ral), sRalfParamLora.sync_word) != RAL_STATUS_OK)
                    {
                        ESP_LOGE(TAG, "Failed to setup lora!");
                        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
                    }
                    ESP_LOGD(TAG, "setup lora syncword successfully");
                }
                break;
            case RAL_PKT_TYPE_GFSK:

                break;
            case RAL_PKT_TYPE_FLRC:

                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                return -1;
        }
    }

    return 0;
}

static int cmd_rf_lora_cad(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (sCurrentModem != RAL_PKT_TYPE_LORA)
    {
        LiotCmdPrint("CAD is only supported in LoRa mode." CMD_ENDL);
        return -1;
    }

    if (argc < 2)
    {
        LiotCmdPrint("Usage: %s" CMD_ENDL, cmd->usage);
        return -1;
    }

    char *endptr;
    uint32_t cad_timeout_ms = (uint32_t)strtoul(argv[1], &endptr, 10);
    if (*endptr != '\0')
    {
        LiotCmdPrint("Invalid CAD timeout value." CMD_ENDL);
        return -1;
    }

    uint32_t rx_timeout_ms = cad_timeout_ms;

    if (argc >= 3)
    {
        rx_timeout_ms = (uint32_t)strtoul(argv[2], &endptr, 10);
        if (*endptr != '\0')
        {
            LiotCmdPrint("Invalid RX timeout value." CMD_ENDL);
            return -1;
        }
    }

    sRxTimeoutAfterCad = rx_timeout_ms;

    LiotCmdPrint("Starting LoRa CAD, CAD timeout: %lu ms, RX timeout: %lu ms" CMD_ENDL, cad_timeout_ms, sRxTimeoutAfterCad);

    ral_lora_cad_params_t cad_params = {
        .cad_symb_nb          = RAL_LORA_CAD_04_SYMB,
        .cad_det_peak_in_symb = 24,
        .cad_det_min_in_symb  = 10,
        .cad_exit_mode        = RAL_LORA_CAD_ONLY,
        .cad_timeout_in_ms    = cad_timeout_ms,
    };

    if (ral_set_lora_cad_params(&(smtc_rac_radio.ral), &cad_params) != RAL_STATUS_OK)
    {
        ESP_LOGE(TAG, "Failed to set CAD params!");
        return -1;
    }

    if (ral_set_lora_cad(&(smtc_rac_radio.ral)) != RAL_STATUS_OK)
    {
        ESP_LOGE(TAG, "Failed to start CAD!");
        return -1;
    }

    ESP_LOGD(TAG, "LoRa CAD started successfully");
    return 0;
}

static int cmd_rf_tx_cw(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    LiotCmdPrint("Transmit CW signal" CMD_ENDL);

    if (ral_set_tx_cw(&(smtc_rac_radio.ral)) != RAL_STATUS_OK)
    {
        ESP_LOGE(TAG, "Failed to set tx cw !");
        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
    }
    ESP_LOGD(TAG, "setup tx cw successfully");

    return 0;
}

static int cmd_rf_tx_preamble(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    LiotCmdPrint("Transmit infinite preamble" CMD_ENDL);

    if (ral_set_tx_infinite_preamble(&(smtc_rac_radio.ral)) != RAL_STATUS_OK)
    {
        ESP_LOGE(TAG, "Failed to set tx preamble !");
        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
    }
    ESP_LOGD(TAG, "setup tx preamble successfully");

    return 0;
}

int LiotRfCmdInit(void)
{
    sRadioEventGroup = xEventGroupCreate();
    assert(sRadioEventGroup);

    xTaskCreate(LiotRadioIrqTask, "LiotRadioIrqTask", 10 * 1024, NULL, 5, NULL);

    return 0;
}

int LiotRfCmdSetPrintfCallback(PrintfCallback callback)
{
    gPrintfCallback = callback;
    return 0;
}

int LiotRfCmdExe(int argc, const char *const *argv)
{
    ESP_LOGD(TAG, "argc %d", argc);
    for (size_t i = 0; i < argc; i++)
    {
        ESP_LOGD(TAG, "argv[%d] = %s", i, argv[i]);
    }

    if (argc == 0)
        return 0;

    for (int i = 0; i < CMD_COUNT; i++)
    {
        if (strcmp(argv[0], cmd_table[i].name) == 0)
        {
            if (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
            {
                LiotCmdPrint("Usage: %s" CMD_ENDL, cmd_table[i].usage ? cmd_table[i].usage : cmd_table[i].name);
                return 0;
            }

            return cmd_table[i].func(&cmd_table[i], argc, argv);
        }
    }

    LiotCmdPrint("Unknown command: %s" CMD_ENDL, argv[0]);
    return -1;
}

char *ComplWorld[CMD_COUNT + 1];

char **LiotRfCmdComplet(int argc, const char *const *argv)
{
    int j = 0;

    ComplWorld[0] = NULL;

    // if there is token in cmdline
    if (argc == 1)
    {
        // get last entered token
        char *bit = (char *)argv[argc - 1];
        // iterate through our available token and match it
        for (int i = 0; i < CMD_COUNT; i++)
        {
            if (strncmp(cmd_table[i].name, bit, strlen(bit)) == 0)
            {
                ComplWorld[j++] = (char *)cmd_table[i].name;
            }
        }
    }
    else if (argc > 1)
    {
    }
    else
    { // if there is no token in cmdline, just print all available token
        for (; j < CMD_COUNT; j++)
        {
            ComplWorld[j] = (char *)cmd_table[j].name;
        }
    }

    // note! last ptr in array always must be NULL!!!
    ComplWorld[j] = NULL;
    // return set of variants
    return ComplWorld;
}

void LiotRfCmdSigint(void)
{
    LiotCmdPrint("^C catched!\n\r");
}
