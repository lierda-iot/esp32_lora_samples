#include "LiotUart.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

static const char *TAG = "LiotUart";

#define UART_PORT UART_NUM_1
#define UART_TX_PIN 47
#define UART_RX_PIN 48
#define UART_BAUD_RATE 921600
#define UART_RX_BUF_SIZE (4 * 1024)
#define UART_TX_BUF_SIZE (4 * 1024)
#define UART_EVENT_QUEUE_SZ 20

#define UART_PACK_LENGTH (2 * 1024)
static uint8_t sUartRxPackCache[UART_PACK_LENGTH] = {0};

#define UART_TX_RING_SIZE (4 * 1024)
#define UART_RX_RING_SIZE (4 * 1024)

static RingbufHandle_t sUartTxRing;
static RingbufHandle_t sUartRxRing;

static LiotUartRxCallback sRxCallback = NULL;
static QueueHandle_t sUartQueue;

void LiotUartTxTask(void *argv)
{
    ESP_LOGI(TAG, "uart Tx task started");

    /* Read from TX ring buffer and send */
    while (1)
    {
        size_t len;
        uint8_t *data = xRingbufferReceive(sUartTxRing, &len, portMAX_DELAY);
        if (data == NULL || len == 0)
        {
            ESP_LOGE(TAG, "no data in uart tx ringbuffer");
            continue;
        }

        ESP_LOGD(TAG, "uart tx %p %d bytes", data, len);
        int writeLen = uart_write_bytes(UART_PORT, (const char *)data, len);
        ESP_LOGD(TAG, "uart write %d bytes", writeLen);

        vRingbufferReturnItem(sUartTxRing, data);
    }

    vTaskDelete(NULL);
}

void LiotUartRxTask(void *argv)
{
    ESP_LOGI(TAG, "uart Rx task started");

    /* Read from RX ring buffer and process */
    while (1)
    {
        size_t len;
        uint8_t *data = xRingbufferReceive(sUartRxRing, &len, portMAX_DELAY);
        if (data == NULL || len == 0)
        {
            ESP_LOGE(TAG, "no data in uart rx ringbuffer");
            continue;
        }

        ESP_LOGD(TAG, "uart receive %p %d bytes", data, len);
        ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_DEBUG);

        if (sRxCallback)
        {
            sRxCallback(data, len);
        }

        vRingbufferReturnItem(sUartRxRing, data);
    }

    vTaskDelete(NULL);
}

void LiotUartEventTask(void *argv)
{
    uart_event_t uartEvent;
    ESP_LOGI(TAG, "uart event task started");

    while (1)
    {
        if (xQueueReceive(sUartQueue, &uartEvent, portMAX_DELAY))
        {
            switch (uartEvent.type)
            {
                case UART_DATA:
                    ESP_LOGD(TAG, "uart data event size %d bytes", uartEvent.size);
                    int len = uart_read_bytes(UART_PORT, sUartRxPackCache, uartEvent.size, 0);
                    ESP_LOGD(TAG, "uart read %d bytes", len);

                    if (xRingbufferSend(sUartRxRing, sUartRxPackCache, len, pdMS_TO_TICKS(10)) != pdTRUE)
                    {
                        ESP_LOGE(TAG, "RX ringbuffer full, drop packet");
                    }
                    break;

                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "UART FIFO overflow");
                    uart_flush_input(UART_PORT);
                    xQueueReset(sUartQueue);
                    break;

                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "UART ring buffer full");
                    uart_flush_input(UART_PORT);
                    xQueueReset(sUartQueue);
                    break;

                case UART_PARITY_ERR:
                    ESP_LOGW(TAG, "UART parity error");
                    break;

                case UART_FRAME_ERR:
                    ESP_LOGW(TAG, "UART frame error");
                    break;

                default:
                    ESP_LOGD(TAG, "UART event type: %d", uartEvent.type);
                    break;
            }
        }
    }

    vTaskDelete(NULL);
}

void LiotUartInit(void)
{
    sUartTxRing = xRingbufferCreate(UART_TX_RING_SIZE, RINGBUF_TYPE_BYTEBUF);
    assert(sUartTxRing != NULL);
    sUartRxRing = xRingbufferCreate(UART_RX_RING_SIZE, RINGBUF_TYPE_BYTEBUF);
    assert(sUartRxRing != NULL);

    ESP_LOGI(TAG, "Uart initialization");

    uart_config_t uart_config = {
        .baud_rate  = UART_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // 1. Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));

    // 2. Configure UART pins
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // 3. Install UART driver
    ESP_ERROR_CHECK(
        uart_driver_install(UART_PORT, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, UART_EVENT_QUEUE_SZ, &sUartQueue, 0));

    ESP_LOGI(TAG, "UART initialized");

    xTaskCreate(LiotUartTxTask, "Lierda_UART_Tx_task", 10 * 1024, NULL, 4, NULL);
    xTaskCreate(LiotUartRxTask, "Lierda_UART_Rx_task", 10 * 1024, NULL, 4, NULL);

    xTaskCreate(LiotUartEventTask, "Lierda_Uart_Event_task", 10 * 1024, NULL, 4, NULL);
}

int LiotUartTx(uint8_t *data, size_t length)
{
    ESP_LOGD(TAG, "Usb Serial Tx %p %d", data, length);
    if (xRingbufferSend(sUartTxRing, data, length, 0) != pdTRUE)
    {
        /* TX ringbuffer full, drop packet */
        ESP_LOGE(TAG, "TX ringbuffer full");
        return -1;
    }

    return 0;
}

void LiotUartSetRxCallback(LiotUartRxCallback callback)
{
    sRxCallback = callback;
}