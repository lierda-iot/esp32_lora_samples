#include "LiotRfCmd.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "microrl_config.h"
#include "LiotLr2021.h"
#include "lr20xx_status.h"
#include "lr20xx_system.h"
#include "lr20xx_radio_common.h"
#include "lr20xx_radio_fifo.h"

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
#define RF_FREQ_IN_HZ 869000000
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

#define RADIO_FIFO_LENGTH 256 /* Radio FIFO size in bytes */

#ifndef FLRC_RAW_BIT_RATE
#define FLRC_RAW_BIT_RATE RAL_FLRC_RAW_BIT_RATE_2_600_MBPS
#endif

#ifndef FLRC_CR
#define FLRC_CR RAL_FLRC_CR_1_1
#endif

#ifndef FLRC_PULSE_SHAPE
#define FLRC_PULSE_SHAPE RAL_FLRC_PULSE_SHAPE_BT_05
#endif

#ifndef FLRC_PREAMBLE_LEN
#define FLRC_PREAMBLE_LEN RAL_FLRC_PREAMBLE_LENGTH_16_BITS
#endif

#ifndef FLRC_SYNCWORD_LEN
#define FLRC_SYNCWORD_LEN RAL_FLRC_SYNCWORD_LENGTH_4_BYTES
#endif

#ifndef FLRC_TX_SYNCWORD
#define FLRC_TX_SYNCWORD RAL_FLRC_TX_SYNCWORD_1
#endif

#ifndef FLRC_MATCH_SYNCWORD
#define FLRC_MATCH_SYNCWORD RAL_FLRC_RX_MATCH_SYNCWORD_1
#endif

#ifndef FLRC_PLD_IS_FIX
#define FLRC_PLD_IS_FIX false
#endif

#ifndef FLRC_CRC
#define FLRC_CRC RAL_FLRC_CRC_2_BYTES
#endif

#ifndef FLRC_CRC_SEED
#define FLRC_CRC_SEED 0x00000000
#endif

#ifndef FLRC_CRC_POLYNOMIAL
#define FLRC_CRC_POLYNOMIAL 0x00000000
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

static uint8_t sFlrcSyncWord1[] = {0x90, 0x56, 0x34, 0x12};
static uint8_t sFlrcSyncWord2[] = {0x90, 0x56, 0x34, 0x12};
static uint8_t sFlrcSyncWord3[] = {0x90, 0x56, 0x34, 0x12};

static ralf_params_flrc_t sRalfParamFlrc = {
    .mod_params =
        {
            .raw_bit_rate = FLRC_RAW_BIT_RATE,
            .cr           = FLRC_CR,
            .pulse_shape  = FLRC_PULSE_SHAPE,
        },
    .pkt_params =
        {
            .preamble_len     = FLRC_PREAMBLE_LEN,
            .sync_word_len    = FLRC_SYNCWORD_LEN,
            .tx_syncword      = FLRC_TX_SYNCWORD,
            .match_sync_word  = FLRC_MATCH_SYNCWORD,
            .pld_is_fix       = FLRC_PLD_IS_FIX,
            .pld_len_in_bytes = RADIO_FIFO_LENGTH,
            .crc_type         = FLRC_CRC,
        },
    .sync_word =
        {
            sFlrcSyncWord1,
            sFlrcSyncWord2,
            sFlrcSyncWord3,
        },
    .rf_freq_in_hz     = RF_FREQ_IN_HZ,
    .crc_seed          = FLRC_CRC_SEED,
    .crc_polynomial    = FLRC_CRC_POLYNOMIAL,
    .output_pwr_in_dbm = TX_OUTPUT_POWER_DBM,
    .is_tx             = false,
};

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

static int rf_parse_hex_payload(int argc, const char *const *argv, uint8_t *payload, size_t payload_size, uint16_t *len)
{
    if (argc < 2)
    {
        return -1;
    }

    size_t parsed_len = argc - 1;
    if (parsed_len > payload_size)
    {
        LiotCmdPrint("Payload too long" CMD_ENDL);
        return -1;
    }

    for (size_t i = 0; i < parsed_len; i++)
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

    *len = (uint16_t)parsed_len;
    return 0;
}

static int rf_setup_flrc(bool is_tx, uint16_t payload_len)
{
    sRalfParamFlrc.is_tx = is_tx;
    sRalfParamFlrc.pkt_params.pld_len_in_bytes = payload_len;

    if (ralf_setup_flrc(&(smtc_rac_radio), &sRalfParamFlrc) != RAL_STATUS_OK)
    {
        ESP_LOGE(TAG, "Failed to setup flrc!");
        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
        return -1;
    }

    ESP_LOGD(TAG, "RALF setup flrc successfully");
    return 0;
}

/* ========================= FLRC burst streaming ========================= */
/* Back-to-back FLRC packet streaming.
 * TX side: fallback mode is FS. Two 511-byte packets are written to the FIFO first; after that, every
 *          TX_DONE issues SET_TX first and then writes one more packet.
 * RX side: continuous RX through set_rx_with_timeout_in_rtc_step(0xFFFFFF). RX_DONE only reads the FIFO
 *          and does not issue SET_RX again.
 * TX and RX use exactly the same FLRC packet parameters. */

#define FLRC_BURST_PAYLOAD_LEN 511 /* Packet length, FLRC payload range is [6:511] */

/* FLRC packet parameters shared by TX and RX, so both sides are configured identically */
#define FLRC_BURST_RAW_BIT_RATE RAL_FLRC_RAW_BIT_RATE_2_600_MBPS
#define FLRC_BURST_CR_DEFAULT   RAL_FLRC_CR_3_4 /* Default coding rate, can be changed with flrc_burst_cr */
#define FLRC_BURST_PULSE_SHAPE  RAL_FLRC_PULSE_SHAPE_BT_05
#define FLRC_BURST_PREAMBLE_LEN RAL_FLRC_PREAMBLE_LENGTH_32_BITS
#define FLRC_BURST_CRC          RAL_FLRC_CRC_2_BYTES
#define FLRC_BURST_CRC_SEED     0xFFFFFFFFU
#define FLRC_BURST_CRC_POLY     0x0000755BU

typedef enum
{
    FLRC_BURST_MODE_NONE = 0,
    FLRC_BURST_MODE_TX,
    FLRC_BURST_MODE_RX,
} flrc_burst_mode_t;

static volatile flrc_burst_mode_t sFlrcBurstMode = FLRC_BURST_MODE_NONE;

/* Burst coding rate, configurable (must be the same on TX and RX) */
static ral_flrc_cr_t sFlrcBurstCr = FLRC_BURST_CR_DEFAULT;

/* RX statistics, reset every second */
static uint32_t          sBurstRxPkts   = 0; /* Packets received in the current second */
static uint32_t          sBurstRxBytes  = 0; /* Bytes received in the current second */
static uint32_t          sBurstRxCrcErr = 0; /* CRC errors in the current second */
static int16_t           sBurstRxRssi   = 0; /* RSSI of the last packet (dBm) */
static esp_timer_handle_t sBurstStatTimer = NULL;

/* TX payload buffer, the incrementing sequence number lets the receiver follow the stream */
static uint8_t  sBurstTxBuf[FLRC_BURST_PAYLOAD_LEN];
static uint32_t sBurstTxSeq   = 0;
static uint32_t sBurstTxPkts  = 0; /* Packets sent in the current second */
static uint32_t sBurstTxBytes = 0; /* Bytes sent in the current second */

static void flrc_burst_fill_tx_payload(void)
{
    sBurstTxBuf[0] = (uint8_t)(sBurstTxSeq >> 24);
    sBurstTxBuf[1] = (uint8_t)(sBurstTxSeq >> 16);
    sBurstTxBuf[2] = (uint8_t)(sBurstTxSeq >> 8);
    sBurstTxBuf[3] = (uint8_t)(sBurstTxSeq);
    for (int i = 4; i < FLRC_BURST_PAYLOAD_LEN; i++)
    {
        sBurstTxBuf[i] = (uint8_t)(i & 0xFF);
    }
    sBurstTxSeq++;
}

/* Print the TX/RX statistics every second, then reset them */
static void flrc_burst_stat_timer_cb(void *arg)
{
    (void)arg;

    if (sFlrcBurstMode == FLRC_BURST_MODE_TX)
    {
        uint32_t pkts  = sBurstTxPkts;
        uint32_t bytes = sBurstTxBytes;

        sBurstTxPkts  = 0;
        sBurstTxBytes = 0;

        LiotCmdPrint("[FLRC BURST TX] %lu pkt/s, %.3f Mbps" CMD_ENDL,
                     (unsigned long)pkts, (double)bytes * 8.0 / 1000000.0);
    }
    else if (sFlrcBurstMode == FLRC_BURST_MODE_RX)
    {
        uint32_t pkts   = sBurstRxPkts;
        uint32_t bytes  = sBurstRxBytes;
        uint32_t crcErr = sBurstRxCrcErr;
        int16_t  rssi   = sBurstRxRssi;

        sBurstRxPkts   = 0;
        sBurstRxBytes  = 0;
        sBurstRxCrcErr = 0;

        LiotCmdPrint("[FLRC BURST RX] %lu pkt/s, %.3f Mbps, RSSI %d dBm, CRC_ERR %lu" CMD_ENDL,
                     (unsigned long)pkts, (double)bytes * 8.0 / 1000000.0, (int)rssi, (unsigned long)crcErr);
    }
}

/* FLRC setup shared by TX and RX: burst packet parameters, 511-byte packets, 1024-byte FIFO, fallback mode */
static int flrc_burst_setup(bool is_tx)
{
    const void *ctx = smtc_rac_radio.ral.context;

    sCurrentModem = RAL_PKT_TYPE_FLRC;

    /* Override with the burst FLRC parameters so that TX and RX match exactly */
    sRalfParamFlrc.mod_params.raw_bit_rate      = FLRC_BURST_RAW_BIT_RATE;
    sRalfParamFlrc.mod_params.cr                = sFlrcBurstCr;
    sRalfParamFlrc.mod_params.pulse_shape       = FLRC_BURST_PULSE_SHAPE;
    sRalfParamFlrc.pkt_params.preamble_len      = FLRC_BURST_PREAMBLE_LEN;
    sRalfParamFlrc.pkt_params.sync_word_len     = RAL_FLRC_SYNCWORD_LENGTH_4_BYTES;
    sRalfParamFlrc.pkt_params.tx_syncword       = RAL_FLRC_TX_SYNCWORD_1;
    sRalfParamFlrc.pkt_params.match_sync_word   = RAL_FLRC_RX_MATCH_SYNCWORD_1;
    sRalfParamFlrc.pkt_params.pld_is_fix        = false;
    sRalfParamFlrc.pkt_params.crc_type          = FLRC_BURST_CRC;
    sRalfParamFlrc.crc_seed                     = FLRC_BURST_CRC_SEED;
    sRalfParamFlrc.crc_polynomial               = FLRC_BURST_CRC_POLY;

    if (rf_setup_flrc(is_tx, FLRC_BURST_PAYLOAD_LEN) != 0)
    {
        return -1;
    }

    /* 2 x 511 = 1022 bytes is larger than the default 256-byte FIFO, so switch to the 1024-byte FIFO */
    if (is_tx)
    {
        if (lr20xx_radio_fifo_configure_1024_byte_tx_fifo(ctx) != LR20XX_STATUS_OK)
        {
            ESP_LOGE(TAG, "cfg 1024 tx fifo failed");
            return -1;
        }
    }
    else
    {
        if (lr20xx_radio_fifo_configure_1024_byte_rx_fifo(ctx) != LR20XX_STATUS_OK)
        {
            ESP_LOGE(TAG, "cfg 1024 rx fifo failed");
            return -1;
        }
    }

    /* Set the DIO interrupts again (ralf_setup_flrc may have changed them) */
    ral_set_dio_irq_params(&(smtc_rac_radio.ral),
                           RAL_IRQ_TX_DONE | RAL_IRQ_RX_DONE | RAL_IRQ_RX_CRC_ERROR | RAL_IRQ_RX_HDR_ERROR);

    /* TX falls back to FS so that packets can be sent back to back */
    lr20xx_radio_common_fallback_modes_t fb = is_tx ? LR20XX_RADIO_FALLBACK_FS : LR20XX_RADIO_FALLBACK_STDBY_XOSC;
    if (lr20xx_radio_common_set_rx_tx_fallback_mode(ctx, fb) != LR20XX_STATUS_OK)
    {
        ESP_LOGE(TAG, "set fallback failed");
        return -1;
    }

    return 0;
}

/* Start the 1 s statistics timer (shared by TX and RX) */
static void flrc_burst_start_stat_timer(void)
{
    if (sBurstStatTimer == NULL)
    {
        const esp_timer_create_args_t targs = {
            .callback = flrc_burst_stat_timer_cb,
            .name     = "flrc_burst_stat",
        };
        esp_timer_create(&targs, &sBurstStatTimer);
    }
    esp_timer_start_periodic(sBurstStatTimer, 1000 * 1000); /* 1s */
}

static int flrc_burst_start_tx(void)
{
    const void *ctx = smtc_rac_radio.ral.context;

    if (flrc_burst_setup(true) != 0)
    {
        return -1;
    }

    sBurstTxSeq   = 0;
    sBurstTxPkts  = 0;
    sBurstTxBytes = 0;
    lr20xx_radio_fifo_clear_tx(ctx);

    /* Write two 511-byte packets into the FIFO first */
    flrc_burst_fill_tx_payload();
    lr20xx_radio_fifo_write_tx(ctx, sBurstTxBuf, FLRC_BURST_PAYLOAD_LEN);
    flrc_burst_fill_tx_payload();
    lr20xx_radio_fifo_write_tx(ctx, sBurstTxBuf, FLRC_BURST_PAYLOAD_LEN);

    sFlrcBurstMode = FLRC_BURST_MODE_TX;

    /* Start TX: the first packet goes out, the second one waits in the FIFO */
    if (lr20xx_radio_common_set_tx(ctx, 0) != LR20XX_STATUS_OK)
    {
        ESP_LOGE(TAG, "set_tx failed");
        sFlrcBurstMode = FLRC_BURST_MODE_NONE;
        return -1;
    }

    flrc_burst_start_stat_timer();

    LiotCmdPrint("FLRC BURST TX started (511B x stream, FALLBACK_FS)" CMD_ENDL);
    return 0;
}

static int flrc_burst_start_rx(void)
{
    const void *ctx = smtc_rac_radio.ral.context;

    if (flrc_burst_setup(false) != 0)
    {
        return -1;
    }

    lr20xx_radio_fifo_clear_rx(ctx);

    sBurstRxPkts   = 0;
    sBurstRxBytes  = 0;
    sBurstRxCrcErr = 0;
    sBurstRxRssi   = 0;
    sFlrcBurstMode = FLRC_BURST_MODE_RX;

    /* Continuous RX: 0xFFFFFF means RX continuous */
    if (lr20xx_radio_common_set_rx_with_timeout_in_rtc_step(ctx, 0xFFFFFF) != LR20XX_STATUS_OK)
    {
        ESP_LOGE(TAG, "set_rx continuous failed");
        sFlrcBurstMode = FLRC_BURST_MODE_NONE;
        return -1;
    }

    flrc_burst_start_stat_timer();

    LiotCmdPrint("FLRC BURST RX started (continuous, print pkt/s)" CMD_ENDL);
    return 0;
}

static void flrc_burst_stop(void)
{
    const void *ctx = smtc_rac_radio.ral.context;

    if (sBurstStatTimer != NULL)
    {
        esp_timer_stop(sBurstStatTimer);
    }
    sFlrcBurstMode = FLRC_BURST_MODE_NONE;

    ral_set_standby(&(smtc_rac_radio.ral), RAL_STANDBY_CFG_XOSC);
    lr20xx_radio_fifo_clear_tx(ctx);
    lr20xx_radio_fifo_clear_rx(ctx);

    /* Drop IRQs raised before standby, so that the IRQ task does not handle them as normal TX/RX events */
    ral_clear_irq_status(&(smtc_rac_radio.ral), RAL_IRQ_ALL);

    LiotCmdPrint("FLRC BURST stopped" CMD_ENDL);
}

/* TX_DONE handler: issue SET_TX first (sends the next packet already in the FIFO), then write one more packet */
static void flrc_burst_on_tx_done(void)
{
    const void *ctx = smtc_rac_radio.ral.context;

    /* One TX_DONE means one packet has been sent */
    sBurstTxPkts++;
    sBurstTxBytes += FLRC_BURST_PAYLOAD_LEN;

    if (lr20xx_radio_common_set_tx(ctx, 0) != LR20XX_STATUS_OK)
    {
        ESP_LOGE(TAG, "burst set_tx failed");
        return;
    }
    flrc_burst_fill_tx_payload();
    lr20xx_radio_fifo_write_tx(ctx, sBurstTxBuf, FLRC_BURST_PAYLOAD_LEN);
}

/* RX_DONE handler: only read the FIFO, do not issue SET_RX again */
static void flrc_burst_on_rx_done(bool crc_error)
{
    const void *ctx = smtc_rac_radio.ral.context;
    uint16_t     level = 0;

    lr20xx_radio_fifo_get_rx_level(ctx, &level);
    if (level > 0)
    {
        static uint8_t rxbuf[FLRC_BURST_PAYLOAD_LEN];
        uint16_t       n = (level > FLRC_BURST_PAYLOAD_LEN) ? FLRC_BURST_PAYLOAD_LEN : level;
        lr20xx_radio_fifo_read_rx(ctx, rxbuf, n);
        sBurstRxBytes += n;
    }

    if (crc_error)
    {
        sBurstRxCrcErr++;
    }
    sBurstRxPkts++;

    ral_flrc_rx_pkt_status_t st;
    if (ral_get_flrc_rx_pkt_status(&(smtc_rac_radio.ral), &st) == RAL_STATUS_OK)
    {
        sBurstRxRssi = st.rssi_avg_in_dbm;
    }
}

static void rf_update_per_status_from_rx(bool success, bool crc_error, bool header_error)
{
    gPerStat.receivePkt++;
    if (success)
    {
        gPerStat.successPkt++;
    }
    if (crc_error)
    {
        gPerStat.crcErr++;
    }
    if (header_error)
    {
        gPerStat.headerErr++;
    }

    switch (sCurrentModem)
    {
        case RAL_PKT_TYPE_LORA:
        {
            ral_lora_rx_pkt_status_t lora_rx_pkt_status;
            if (ral_get_lora_rx_pkt_status(&(smtc_rac_radio.ral), &lora_rx_pkt_status) == RAL_STATUS_OK)
            {
                gPerStat.signalRssi = lora_rx_pkt_status.signal_rssi_pkt_in_dbm;
                gPerStat.snr        = lora_rx_pkt_status.snr_pkt_in_db;
            }
            break;
        }
        case RAL_PKT_TYPE_FLRC:
        {
            ral_flrc_rx_pkt_status_t flrc_rx_pkt_status;
            if (ral_get_flrc_rx_pkt_status(&(smtc_rac_radio.ral), &flrc_rx_pkt_status) == RAL_STATUS_OK)
            {
                gPerStat.signalRssi = flrc_rx_pkt_status.rssi_avg_in_dbm;
                gPerStat.snr        = 0;
            }
            break;
        }
        default:
            break;
    }

    per_print();
}

/* LoRa sleep status: false = awake, true = sleeping */
static uint8_t gLoraSleepStatus = false;

static uint32_t sRxTimeoutAfterCad = 0;

typedef struct
{
    uint8_t xta;
    uint8_t xtb;
    uint8_t wait_time_us;
    bool configured;
} rf_xosc_cfg_t;

static rf_xosc_cfg_t sRfXoscCfg = {0};

#define EVT_RADIO_GPIO_INT BIT0 /* Radio GPIO interrupt event */

static EventGroupHandle_t sRadioEventGroup;

/* The radio driver and its SPI HAL are not thread-safe. CLI commands and the radio IRQ task both access the radio,
 * so each command and each IRQ handling holds this lock. */
static SemaphoreHandle_t sRadioMutex;

static int rf_apply_xosc_config(void)
{
#if !defined(CONFIG_SMTC_RADIO_LR20XX)
    ESP_LOGW(TAG, "XOSC configuration is only supported on LR20XX");
    return -1;
#else
    if (sRfXoscCfg.configured == false)
    {
        return 0;
    }

    lr20xx_status_t status = lr20xx_system_configure_xosc(smtc_rac_radio.ral.context,
                                                          sRfXoscCfg.xta,
                                                          sRfXoscCfg.xtb,
                                                          sRfXoscCfg.wait_time_us);
    if (status != LR20XX_STATUS_OK)
    {
        ESP_LOGE(TAG,
                 "Failed to configure XOSC, status=%d, xta=%u, xtb=%u, wait_time_us=%u",
                 status,
                 sRfXoscCfg.xta,
                 sRfXoscCfg.xtb,
                 sRfXoscCfg.wait_time_us);
        return -1;
    }

    ESP_LOGI(TAG,
             "XOSC configured, xta=%u, xtb=%u, wait_time_us=%u",
             sRfXoscCfg.xta,
             sRfXoscCfg.xtb,
             sRfXoscCfg.wait_time_us);
    return 0;
#endif
}

void RadioIrqCallback(void *context)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Set the event bit */
    xEventGroupSetBitsFromISR(sRadioEventGroup, EVT_RADIO_GPIO_INT, &xHigherPriorityTaskWoken);

    /* Request a context switch if a higher-priority task was woken */
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

    if (rf_apply_xosc_config() != 0)
    {
        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
    }

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
            if (rf_setup_flrc(false, RADIO_FIFO_LENGTH) != 0)
            {
                return;
            }
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

void LiotRadioIrqTask(void *argv)
{
    uint16_t receivedPackSize = 0;
    uint8_t receivedPackBuff[RADIO_FIFO_LENGTH];
    ral_irq_t radioIrq = 0;

    ESP_LOGI(TAG, "LR2021 task started");

    xSemaphoreTake(sRadioMutex, portMAX_DELAY);
    LiotRfCmdRfResetAndInit();
    xSemaphoreGive(sRadioMutex);

    while (1)
    {
        EventBits_t bits = xEventGroupWaitBits(sRadioEventGroup,
                                               EVT_RADIO_GPIO_INT, /* Bits to wait for */
                                               pdTRUE,             /* Clear the bits on exit */
                                               pdFALSE,            /* Wait for any bit */
                                               portMAX_DELAY       /* Block forever */
        );

        if (bits & EVT_RADIO_GPIO_INT)
        {
            xSemaphoreTake(sRadioMutex, portMAX_DELAY);

            ESP_LOGD(TAG, "radio interrupt event received");
            if (ral_get_and_clear_irq_status(&(smtc_rac_radio.ral), &radioIrq) != RAL_STATUS_OK)
            {
                SMTC_MODEM_HAL_PANIC();
            }
            /* FLRC burst mode is handled separately by the low-level streaming handlers */
            if (sFlrcBurstMode == FLRC_BURST_MODE_TX)
            {
                if ((radioIrq & RAL_IRQ_TX_DONE) == RAL_IRQ_TX_DONE)
                {
                    flrc_burst_on_tx_done();
                }
                xSemaphoreGive(sRadioMutex);
                continue;
            }
            else if (sFlrcBurstMode == FLRC_BURST_MODE_RX)
            {
                if ((radioIrq & (RAL_IRQ_RX_DONE | RAL_IRQ_RX_CRC_ERROR)) != 0)
                {
                    flrc_burst_on_rx_done((radioIrq & RAL_IRQ_RX_CRC_ERROR) == RAL_IRQ_RX_CRC_ERROR);
                }
                xSemaphoreGive(sRadioMutex);
                continue;
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

                /* Restart RX, which also clears the FIFO */
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
                    rf_update_per_status_from_rx(false, true, false);
                }
                else
                {
                    rf_update_per_status_from_rx(true, false, false);
                }
            }
            else if ((radioIrq & RAL_IRQ_RX_TIMEOUT) == RAL_IRQ_RX_TIMEOUT)
            {
                ESP_LOGW(TAG, "RX timeout irq received");
            }
            else if ((radioIrq & RAL_IRQ_RX_HDR_ERROR) == RAL_IRQ_RX_HDR_ERROR)
            {
                ESP_LOGE(TAG, "RX header error irq received");

                rf_update_per_status_from_rx(false, false, true);
            }
            else if ((radioIrq & RAL_IRQ_CAD_OK) == RAL_IRQ_CAD_OK)
            {
                LiotCmdPrint("CAD Positive (Preamble Detected)!" CMD_ENDL);
                /* CAD detected activity, start RX */
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

            xSemaphoreGive(sRadioMutex);
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

    const char *usage; // Argument format
    const char *desc;  // Description
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
static int cmd_rf_xosc(const cli_cmd_t *cmd, int argc, const char *const *argv);

static int cmd_rf_lora_sf(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_lora_bw(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_lora_cr(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_lora_preamble(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_lora_syncword(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_lora_cad(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_flrc_br(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_flrc_cr(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_flrc_preamble(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_flrc_crc(const cli_cmd_t *cmd, int argc, const char *const *argv);

typedef struct
{
    const char *name;
    ral_flrc_raw_bit_rate_t br;
} flrc_br_map_t;

static const flrc_br_map_t flrc_br_table[] = {
    {"260", RAL_FLRC_RAW_BIT_RATE_0_260_MBPS},
    {"325", RAL_FLRC_RAW_BIT_RATE_0_325_MBPS},
    {"520", RAL_FLRC_RAW_BIT_RATE_0_520_MBPS},
    {"650", RAL_FLRC_RAW_BIT_RATE_0_650_MBPS},
    {"1040", RAL_FLRC_RAW_BIT_RATE_1_040_MBPS},
    {"1300", RAL_FLRC_RAW_BIT_RATE_1_300_MBPS},
    {"2080", RAL_FLRC_RAW_BIT_RATE_2_080_MBPS},
    {"2600", RAL_FLRC_RAW_BIT_RATE_2_600_MBPS},
};

static int parse_flrc_br(const char *str, ral_flrc_raw_bit_rate_t *out)
{
    for (size_t i = 0; i < sizeof(flrc_br_table) / sizeof(flrc_br_table[0]); i++)
    {
        if (strcmp(str, flrc_br_table[i].name) == 0)
        {
            *out = flrc_br_table[i].br;
            return 0;
        }
    }

    return -1;
}

typedef struct
{
    const char *name;
    ral_flrc_cr_t cr;
} flrc_cr_map_t;

static const flrc_cr_map_t flrc_cr_table[] = {
    {"1/2", RAL_FLRC_CR_1_2},
    {"3/4", RAL_FLRC_CR_3_4},
    {"1/1", RAL_FLRC_CR_1_1},
    {"2/3", RAL_FLRC_CR_2_3},
};

static int parse_flrc_cr(const char *str, ral_flrc_cr_t *out)
{
    for (size_t i = 0; i < sizeof(flrc_cr_table) / sizeof(flrc_cr_table[0]); i++)
    {
        if (strcmp(str, flrc_cr_table[i].name) == 0)
        {
            *out = flrc_cr_table[i].cr;
            return 0;
        }
    }

    return -1;
}

typedef struct
{
    const char *name;
    ral_flrc_preamble_length_t preamble;
} flrc_preamble_map_t;

static const flrc_preamble_map_t flrc_preamble_table[] = {
    {"4", RAL_FLRC_PREAMBLE_LENGTH_4_BITS},
    {"8", RAL_FLRC_PREAMBLE_LENGTH_8_BITS},
    {"12", RAL_FLRC_PREAMBLE_LENGTH_12_BITS},
    {"16", RAL_FLRC_PREAMBLE_LENGTH_16_BITS},
    {"20", RAL_FLRC_PREAMBLE_LENGTH_20_BITS},
    {"24", RAL_FLRC_PREAMBLE_LENGTH_24_BITS},
    {"28", RAL_FLRC_PREAMBLE_LENGTH_28_BITS},
    {"32", RAL_FLRC_PREAMBLE_LENGTH_32_BITS},
};

static int parse_flrc_preamble(const char *str, ral_flrc_preamble_length_t *out)
{
    for (size_t i = 0; i < sizeof(flrc_preamble_table) / sizeof(flrc_preamble_table[0]); i++)
    {
        if (strcmp(str, flrc_preamble_table[i].name) == 0)
        {
            *out = flrc_preamble_table[i].preamble;
            return 0;
        }
    }

    return -1;
}

typedef struct
{
    const char *name;
    ral_flrc_crc_type_t crc;
} flrc_crc_map_t;

static const flrc_crc_map_t flrc_crc_table[] = {
    {"off", RAL_FLRC_CRC_OFF},
    {"2", RAL_FLRC_CRC_2_BYTES},
    {"3", RAL_FLRC_CRC_3_BYTES},
    {"4", RAL_FLRC_CRC_4_BYTES},
};

static int parse_flrc_crc(const char *str, ral_flrc_crc_type_t *out)
{
    for (size_t i = 0; i < sizeof(flrc_crc_table) / sizeof(flrc_crc_table[0]); i++)
    {
        if (strcmp(str, flrc_crc_table[i].name) == 0)
        {
            *out = flrc_crc_table[i].crc;
            return 0;
        }
    }

    return -1;
}

static int rf_apply_flrc_if_current(void)
{
    if (sCurrentModem != RAL_PKT_TYPE_FLRC)
    {
        return 0;
    }

    return rf_setup_flrc(false, sRalfParamFlrc.pkt_params.pld_len_in_bytes);
}

static int cmd_rf_flrc_br(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc != 1 && argc != 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        LiotCmdPrint("Current flrc br: %s" CMD_ENDL,
                     ral_flrc_raw_bit_rate_to_str(sRalfParamFlrc.mod_params.raw_bit_rate));
        return 0;
    }

    ral_flrc_raw_bit_rate_t br;
    if (parse_flrc_br(argv[1], &br) != 0)
    {
        LiotCmdPrint("Invalid flrc br: %s" CMD_ENDL, argv[1]);
        return -1;
    }

    sRalfParamFlrc.mod_params.raw_bit_rate = br;
    return rf_apply_flrc_if_current();
}

static int cmd_rf_flrc_cr(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc != 1 && argc != 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        LiotCmdPrint("Current flrc cr: %s" CMD_ENDL, ral_flrc_cr_to_str(sRalfParamFlrc.mod_params.cr));
        return 0;
    }

    ral_flrc_cr_t cr;
    if (parse_flrc_cr(argv[1], &cr) != 0)
    {
        LiotCmdPrint("Invalid flrc cr: %s" CMD_ENDL, argv[1]);
        return -1;
    }

    sRalfParamFlrc.mod_params.cr = cr;
    return rf_apply_flrc_if_current();
}

static int cmd_rf_flrc_preamble(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc != 1 && argc != 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        LiotCmdPrint("Current flrc preamble: %s" CMD_ENDL,
                     ral_flrc_preamble_length_to_str(sRalfParamFlrc.pkt_params.preamble_len));
        return 0;
    }

    ral_flrc_preamble_length_t preamble;
    if (parse_flrc_preamble(argv[1], &preamble) != 0)
    {
        LiotCmdPrint("Invalid flrc preamble: %s" CMD_ENDL, argv[1]);
        return -1;
    }

    sRalfParamFlrc.pkt_params.preamble_len = preamble;
    return rf_apply_flrc_if_current();
}

static int cmd_rf_flrc_crc(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc != 1 && argc != 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        LiotCmdPrint("Current flrc crc: %s" CMD_ENDL, ral_flrc_crc_type_to_str(sRalfParamFlrc.pkt_params.crc_type));
        return 0;
    }

    ral_flrc_crc_type_t crc;
    if (parse_flrc_crc(argv[1], &crc) != 0)
    {
        LiotCmdPrint("Invalid flrc crc: %s" CMD_ENDL, argv[1]);
        return -1;
    }

    sRalfParamFlrc.pkt_params.crc_type = crc;
    return rf_apply_flrc_if_current();
}

static int cmd_rf_tx_cw(const cli_cmd_t *cmd, int argc, const char *const *argv);
static int cmd_rf_tx_preamble(const cli_cmd_t *cmd, int argc, const char *const *argv);

/* Optional second argument: coding rate <1/2|3/4|1/1|2/3>; the current value is used when omitted */
static int cmd_flrc_burst_parse_optional_cr(int argc, const char *const *argv)
{
    if (argc >= 2)
    {
        ral_flrc_cr_t cr;
        if (parse_flrc_cr(argv[1], &cr) != 0)
        {
            LiotCmdPrint("Invalid flrc cr: %s" CMD_ENDL, argv[1]);
            return -1;
        }
        sFlrcBurstCr = cr;
    }
    return 0;
}

static int cmd_flrc_burst_tx(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    (void)cmd;
    if (cmd_flrc_burst_parse_optional_cr(argc, argv) != 0)
    {
        return -1;
    }
    return flrc_burst_start_tx();
}

static int cmd_flrc_burst_rx(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    (void)cmd;
    if (cmd_flrc_burst_parse_optional_cr(argc, argv) != 0)
    {
        return -1;
    }
    return flrc_burst_start_rx();
}

static int cmd_flrc_burst_cr(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    if (argc != 1 && argc != 2)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        LiotCmdPrint("Current flrc burst cr: %s" CMD_ENDL, ral_flrc_cr_to_str(sFlrcBurstCr));
        return 0;
    }

    ral_flrc_cr_t cr;
    if (parse_flrc_cr(argv[1], &cr) != 0)
    {
        LiotCmdPrint("Invalid flrc cr: %s" CMD_ENDL, argv[1]);
        return -1;
    }

    sFlrcBurstCr = cr;
    LiotCmdPrint("flrc burst cr set to %s (take effect on next flrc_burst_tx/rx)" CMD_ENDL,
                 ral_flrc_cr_to_str(sFlrcBurstCr));
    return 0;
}

static int cmd_flrc_burst_stop(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    (void)cmd;
    (void)argc;
    (void)argv;
    flrc_burst_stop();
    return 0;
}

static const cli_cmd_t cmd_table[] = {
    {"help", cmd_help, NULL, 0, NULL, "Show command list and usage"},
    {"clear", cmd_clear, NULL, 0, NULL, "Clear terminal screen"},
    {"list", cmd_list, NULL, 0, NULL, "List items"},
    {"info", cmd_info, NULL, 0, NULL, "Show product and firmware information"},

    /* General RF commands */
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
    {"xosc", cmd_rf_xosc, NULL, 0, "xosc <xta> <xtb> <wait_time_us>", "Get/Set LR20xx XOSC configuration"},

    /* LoRa parameters */
    {"lora_sf", cmd_rf_lora_sf, NULL, 0, "lora_sf <5~12>", "Get/Set LoRa spreading factor"},
    {"lora_bw", cmd_rf_lora_bw, NULL, 0, "lora_bw <31|62|125|250|500|1000>", "Get/Set LoRa bandwidth"},
    {"lora_cr", cmd_rf_lora_cr, NULL, 0, "lora_cr <4/5|4/6|4/7|4/8|li4/5|li4/6|li4/8>", "Get/Set LoRa coding rate"},
    {"lora_preamble", cmd_rf_lora_preamble, NULL, 0, "lora_preamble <length>", "Get/Set LoRa preamble length"},
    {"lora_syncword", cmd_rf_lora_syncword, NULL, 0, "lora_syncword <0x12|0x34>", "Get/Set LoRa sync word"},
    {"lora_cad", cmd_rf_lora_cad, NULL, 0, "lora_cad <cad_timeout_ms> [rx_timeout_ms]", "Start LoRa CAD detect"},

    /* FLRC parameters */
    {"flrc_br", cmd_rf_flrc_br, NULL, 0, "flrc_br <260|325|520|650|1040|1300|2080|2600>", "Get/Set FLRC bitrate"},
    {"flrc_cr", cmd_rf_flrc_cr, NULL, 0, "flrc_cr <1/2|3/4|1/1|2/3>", "Get/Set FLRC coding rate"},
    {"flrc_preamble", cmd_rf_flrc_preamble, NULL, 0, "flrc_preamble <4|8|12|16|20|24|28|32>", "Get/Set FLRC preamble length"},
    {"flrc_crc", cmd_rf_flrc_crc, NULL, 0, "flrc_crc <off|2|3|4>", "Get/Set FLRC CRC bytes"},

    /* FLRC burst streaming */
    {"flrc_burst_tx", cmd_flrc_burst_tx, NULL, 0, "flrc_burst_tx [1/2|3/4|1/1|2/3]",
     "Start FLRC burst TX stream (511B x N), optional CR"},
    {"flrc_burst_rx", cmd_flrc_burst_rx, NULL, 0, "flrc_burst_rx [1/2|3/4|1/1|2/3]",
     "Start FLRC burst RX (print pkt/s), optional CR"},
    {"flrc_burst_cr", cmd_flrc_burst_cr, NULL, 0, "flrc_burst_cr <1/2|3/4|1/1|2/3>", "Get/Set FLRC burst coding rate"},
    {"flrc_burst_stop", cmd_flrc_burst_stop, NULL, 0, NULL, "Stop FLRC burst TX/RX"},

    /* Test modes */
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
            if (sRfXoscCfg.configured)
            {
                LiotCmdPrint("xosc        : xta=%u xtb=%u wait=%u us" CMD_ENDL,
                             sRfXoscCfg.xta,
                             sRfXoscCfg.xtb,
                             sRfXoscCfg.wait_time_us);
            }
            else
            {
                LiotCmdPrint("xosc        : not configured" CMD_ENDL);
            }
            break;
        case RAL_PKT_TYPE_GFSK:

            break;
        case RAL_PKT_TYPE_FLRC:
            LiotCmdPrint("sync word   : %02X%02X%02X%02X" CMD_ENDL,
                         sFlrcSyncWord1[0],
                         sFlrcSyncWord1[1],
                         sFlrcSyncWord1[2],
                         sFlrcSyncWord1[3]);
            LiotCmdPrint("frequency   : %lu Hz" CMD_ENDL, sRalfParamFlrc.rf_freq_in_hz);
            LiotCmdPrint("power       : %d dBm" CMD_ENDL, sRalfParamFlrc.output_pwr_in_dbm);
            LiotCmdPrint("br          : %s" CMD_ENDL, ral_flrc_raw_bit_rate_to_str(sRalfParamFlrc.mod_params.raw_bit_rate));
            LiotCmdPrint("cr          : %s" CMD_ENDL, ral_flrc_cr_to_str(sRalfParamFlrc.mod_params.cr));
            LiotCmdPrint("preamble    : %s" CMD_ENDL, ral_flrc_preamble_length_to_str(sRalfParamFlrc.pkt_params.preamble_len));
            LiotCmdPrint("crc         : %s" CMD_ENDL, ral_flrc_crc_type_to_str(sRalfParamFlrc.pkt_params.crc_type));
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
                LiotCmdPrint("Current lora sync word: 0x%02X" CMD_ENDL, sRalfParamLora.sync_word);
                break;
            case RAL_PKT_TYPE_GFSK:
                LiotCmdPrint("GFSK sync word is not supported yet" CMD_ENDL);
                break;
            case RAL_PKT_TYPE_FLRC:
                LiotCmdPrint("Current flrc sync word: %02X%02X%02X%02X" CMD_ENDL,
                             sFlrcSyncWord1[0],
                             sFlrcSyncWord1[1],
                             sFlrcSyncWord1[2],
                             sFlrcSyncWord1[3]);
                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                break;
        }
        return 0;
    }

    switch (sCurrentModem)
    {
        case RAL_PKT_TYPE_LORA:
        {
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
        }
        case RAL_PKT_TYPE_GFSK:
            LiotCmdPrint("GFSK sync word is not supported yet" CMD_ENDL);
            return -1;
        case RAL_PKT_TYPE_FLRC:
        {
            char *endptr;
            unsigned long sync_word = strtoul(argv[1], &endptr, 16);
            if (*endptr != '\0' || sync_word > 0xFFFFFFFFUL)
            {
                LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
                return -1;
            }

            uint8_t sync_word_bytes[4] = {
                (uint8_t)((sync_word >> 24) & 0xFF),
                (uint8_t)((sync_word >> 16) & 0xFF),
                (uint8_t)((sync_word >> 8) & 0xFF),
                (uint8_t)(sync_word & 0xFF),
            };
            memcpy(sFlrcSyncWord1, sync_word_bytes, sizeof(sFlrcSyncWord1));
            memcpy(sFlrcSyncWord2, sync_word_bytes, sizeof(sFlrcSyncWord2));
            memcpy(sFlrcSyncWord3, sync_word_bytes, sizeof(sFlrcSyncWord3));

            if (rf_setup_flrc(false, sRalfParamFlrc.pkt_params.pld_len_in_bytes) != 0)
            {
                return -1;
            }
            ESP_LOGD(TAG, "setup flrc sync word successfully");
            break;
        }
        default:
            LiotCmdPrint("Invalid modem type" CMD_ENDL);
            return -1;
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
                LiotCmdPrint("Current lora frequency: %lu Hz" CMD_ENDL, sRalfParamLora.rf_freq_in_hz);
                break;
            case RAL_PKT_TYPE_GFSK:
                LiotCmdPrint("GFSK frequency is not supported yet" CMD_ENDL);
                break;
            case RAL_PKT_TYPE_FLRC:
                LiotCmdPrint("Current flrc frequency: %lu Hz" CMD_ENDL, sRalfParamFlrc.rf_freq_in_hz);
                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                break;
        }
        return 0;
    }

    char *endptr;
    uint32_t rf_freq_in_hz = (uint32_t)strtoul(argv[1], &endptr, 10);
    if (*endptr != '\0')
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    switch (sCurrentModem)
    {
        case RAL_PKT_TYPE_LORA:
            sRalfParamLora.rf_freq_in_hz = rf_freq_in_hz;
            break;
        case RAL_PKT_TYPE_GFSK:
            LiotCmdPrint("GFSK frequency is not supported yet" CMD_ENDL);
            return -1;
        case RAL_PKT_TYPE_FLRC:
            sRalfParamFlrc.rf_freq_in_hz = rf_freq_in_hz;
            break;
        default:
            LiotCmdPrint("Invalid modem type" CMD_ENDL);
            return -1;
    }

    if (ral_set_rf_freq(&(smtc_rac_radio.ral), rf_freq_in_hz) != RAL_STATUS_OK)
    {
        ESP_LOGE(TAG, "Failed to setup freq!");
        SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
    }
    ESP_LOGD(TAG, "setup freq successfully");

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
                LiotCmdPrint("Current lora power: %d dBm" CMD_ENDL, sRalfParamLora.output_pwr_in_dbm);
                break;
            case RAL_PKT_TYPE_GFSK:
                LiotCmdPrint("GFSK power is not supported yet" CMD_ENDL);
                break;
            case RAL_PKT_TYPE_FLRC:
                LiotCmdPrint("Current flrc power: %d dBm" CMD_ENDL, sRalfParamFlrc.output_pwr_in_dbm);
                break;
            default:
                LiotCmdPrint("Invalid modem type" CMD_ENDL);
                break;
        }
        return 0;
    }

    char *endptr;
    long output_pwr_in_dbm = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || output_pwr_in_dbm < INT8_MIN || output_pwr_in_dbm > INT8_MAX)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    switch (sCurrentModem)
    {
        case RAL_PKT_TYPE_LORA:
            sRalfParamLora.output_pwr_in_dbm = (int8_t)output_pwr_in_dbm;
            if (ral_set_tx_cfg(&(smtc_rac_radio.ral),
                               sRalfParamLora.output_pwr_in_dbm,
                               sRalfParamLora.rf_freq_in_hz) != RAL_STATUS_OK)
            {
                ESP_LOGE(TAG, "Failed to setup lora power!");
                SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
            }
            break;
        case RAL_PKT_TYPE_GFSK:
            LiotCmdPrint("GFSK power is not supported yet" CMD_ENDL);
            return -1;
        case RAL_PKT_TYPE_FLRC:
            sRalfParamFlrc.output_pwr_in_dbm = (int8_t)output_pwr_in_dbm;
            if (ral_set_tx_cfg(&(smtc_rac_radio.ral),
                               sRalfParamFlrc.output_pwr_in_dbm,
                               sRalfParamFlrc.rf_freq_in_hz) != RAL_STATUS_OK)
            {
                ESP_LOGE(TAG, "Failed to setup flrc power!");
                SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
            }
            break;
        default:
            LiotCmdPrint("Invalid modem type" CMD_ENDL);
            return -1;
    }

    ESP_LOGD(TAG, "setup power successfully");
    return 0;
}

static int cmd_rf_tx(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
    uint8_t payload[RADIO_FIFO_LENGTH];
    uint16_t len = 0;

    if (rf_parse_hex_payload(argc, argv, payload, sizeof(payload), &len) != 0)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    LiotCmdPrint("TX %d bytes:" CMD_ENDL, len);
    for (int i = 0; i < len; i++)
    {
        LiotCmdPrint("%02X ", payload[i]);
    }
    LiotCmdPrint(CMD_ENDL);

    switch (sCurrentModem)
    {
        case RAL_PKT_TYPE_LORA:
            sRalfParamLora.pkt_params.pld_len_in_bytes = len;
            if (ralf_setup_lora(&(smtc_rac_radio), &sRalfParamLora) != RAL_STATUS_OK)
            {
                ESP_LOGE(TAG, "Failed to setup lora!");
                SMTC_MODEM_HAL_PANIC_ON_FAILURE(false);
            }
            ESP_LOGD(TAG, "RALF setup lora successfully");
            break;
        case RAL_PKT_TYPE_GFSK:
            LiotCmdPrint("GFSK TX is not supported yet" CMD_ENDL);
            return -1;
        case RAL_PKT_TYPE_FLRC:
            if (rf_setup_flrc(true, len) != 0)
            {
                return -1;
            }
            break;
        default:
            LiotCmdPrint("Invalid modem type" CMD_ENDL);
            return -1;
    }

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
            LiotCmdPrint("GFSK RX is not supported yet" CMD_ENDL);
            return -1;
        case RAL_PKT_TYPE_FLRC:
            if (rf_setup_flrc(false, RADIO_FIFO_LENGTH) != 0)
            {
                return -1;
            }

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
                    LiotCmdPrint("GFSK RX is not supported yet" CMD_ENDL);
                    return -1;
                case RAL_PKT_TYPE_FLRC:
                    if (rf_setup_flrc(false, RADIO_FIFO_LENGTH) != 0)
                    {
                        return -1;
                    }

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

static int cmd_rf_xosc(const cli_cmd_t *cmd, int argc, const char *const *argv)
{
#if !defined(CONFIG_SMTC_RADIO_LR20XX)
    LiotCmdPrint("XOSC configuration is only supported on LR20XX" CMD_ENDL);
    return -1;
#else
    if (argc != 1 && argc != 4)
    {
        LiotCmdPrint("%s" CMD_ENDL, cmd->usage);
        return -1;
    }

    if (argc == 1)
    {
        if (sRfXoscCfg.configured)
        {
            LiotCmdPrint("Current XOSC config: xta=%u xtb=%u wait_time_us=%u" CMD_ENDL,
                         sRfXoscCfg.xta,
                         sRfXoscCfg.xtb,
                         sRfXoscCfg.wait_time_us);
        }
        else
        {
            LiotCmdPrint("Current XOSC config: not configured" CMD_ENDL);
        }
        return 0;
    }

    char *endptr;
    unsigned long xta = strtoul(argv[1], &endptr, 0);
    if (*endptr != '\0' || xta > UINT8_MAX)
    {
        LiotCmdPrint("Invalid xta value: %s" CMD_ENDL, argv[1]);
        return -1;
    }

    unsigned long xtb = strtoul(argv[2], &endptr, 0);
    if (*endptr != '\0' || xtb > UINT8_MAX)
    {
        LiotCmdPrint("Invalid xtb value: %s" CMD_ENDL, argv[2]);
        return -1;
    }

    unsigned long wait_time_us = strtoul(argv[3], &endptr, 0);
    if (*endptr != '\0' || wait_time_us > UINT8_MAX)
    {
        LiotCmdPrint("Invalid wait_time_us value: %s" CMD_ENDL, argv[3]);
        return -1;
    }

    sRfXoscCfg.xta          = (uint8_t)xta;
    sRfXoscCfg.xtb          = (uint8_t)xtb;
    sRfXoscCfg.wait_time_us = (uint8_t)wait_time_us;
    sRfXoscCfg.configured   = true;

    if (rf_apply_xosc_config() != 0)
    {
        LiotCmdPrint("Failed to configure XOSC" CMD_ENDL);
        return -1;
    }

    LiotCmdPrint("XOSC configured: xta=%u xtb=%u wait_time_us=%u" CMD_ENDL,
                 sRfXoscCfg.xta,
                 sRfXoscCfg.xtb,
                 sRfXoscCfg.wait_time_us);
    return 0;
#endif
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

    /* Configure the CAD parameters */
    ral_lora_cad_params_t cad_params = {
        .cad_symb_nb          = RAL_LORA_CAD_04_SYMB, /* Detect over 4 symbols */
        .cad_det_peak_in_symb = 24,                   /* Empirical value */
        .cad_det_min_in_symb  = 10,
        .cad_exit_mode        = RAL_LORA_CAD_ONLY,    /* Report CAD_DONE after the scan */
        .cad_timeout_in_ms    = cad_timeout_ms,       /* CAD timeout */
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

    sRadioMutex = xSemaphoreCreateMutex();
    assert(sRadioMutex);

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

            /* Do not let the command interleave with the radio IRQ task */
            xSemaphoreTake(sRadioMutex, portMAX_DELAY);
            int ret = cmd_table[i].func(&cmd_table[i], argc, argv);
            xSemaphoreGive(sRadioMutex);
            return ret;
        }
    }

    LiotCmdPrint("Unknown command: %s" CMD_ENDL, argv[0]);
    return -1;
}

// array for comletion
char *ComplWorld[CMD_COUNT + 1];

// completion callback for microrl library
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
