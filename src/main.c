/**
 * This file is originally based on Espressif's "UART Events Example".
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "soc/uart_reg.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#include "sdkconfig.h"
#include "wifi_helper.h"
#include "syslog_client.h"

static const char *TAG = "uart_events";

#define PATTERN_CHR        '\n'
#define PATTERN_CHR_NUM    (1)         /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/

#define LINE_BUF_SIZE 200
#define UART_BUF_SIZE (10 * 1024)
#define UART_QUEUE_SIZE 100

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

typedef struct
{
    const char *syslog_task_name;
    uart_port_t uart_port;
    QueueHandle_t uart_queue;
} task_params_t;

static QueueHandle_t uart1_queue;

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    const char *error_msg;
    uart_event_type_t last_event_type = UART_DATA;

    task_params_t *params = (task_params_t *)pvParameters;

    char *buffer = build_syslog_client_header(SYSLOG_INFO,
                                              CONFIG_SYSLOG_APP_NAME,
                                              params->syslog_task_name);
    size_t header_len = strlen(buffer);
    buffer = realloc(buffer, header_len + LINE_BUF_SIZE + PATTERN_CHR_NUM + 1);
    char *msg = buffer + header_len;

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    error_msg = "[start uart console logging]";
    ESP_LOGI(TAG, "%s", error_msg);
    strcpy(msg, error_msg);
    syslog_client_send_with_header(buffer, -1);

    for (;;) {
        //Waiting for UART event.
        if (xQueueReceive(params->uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) {
            //ESP_LOGD(TAG, "uart[%d] event:", params->uart_port);
            switch (event.type) {
            // newline detected
            case UART_PATTERN_DET:
                //uart_get_buffered_data_len(params->uart_port, &buffered_size);
                int pos = uart_pattern_pop_pos(params->uart_port);
                //ESP_LOGD(TAG, "[UART PATTERN DETECTED] pos: %d", pos);
                if (pos >= 0)
                {
                    uart_read_bytes(params->uart_port, msg, pos + PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                    if (pos > 0)
                    {
                        while ((pos > 0) && (buffer[header_len + pos - 1] == '\r'))
                        {
                            // strip trailing carriage return
                            pos -= 1;
                        }
                        if (pos > 0)
                        {
                            syslog_client_send_with_header(buffer, header_len + pos);
                        }
                    }
                    //ESP_LOGD(TAG, "read data: %.*s", pos, msg);
                }
                else
                {
                    // the pattern position queue is full so that it cannot
                    // record the position. We should set a larger queue size.
                    const char *error_msg = "[Pattern position buffer full, expect missing lines!]";
                    ESP_LOGW(TAG, "%s", error_msg);
                    strcpy(msg, error_msg);
                    syslog_client_send_with_header(buffer, -1);
                    // As an example, we directly flush the rx buffer here.
                    uart_flush_input(params->uart_port);
                }
                break;
            //Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                error_msg = "[hw fifo overflow]";
                ESP_LOGW(TAG, "%s", error_msg);
                strcpy(msg, error_msg);
                syslog_client_send_with_header(buffer, -1);
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(params->uart_port);
                xQueueReset(uart1_queue);
                break;
            //Event of UART ring buffer full
            case UART_BUFFER_FULL:
                // If buffer full happened, you should consider increasing your buffer size
                error_msg = "[ring buffer full]";
                ESP_LOGW(TAG, "%s", error_msg);
                strcpy(msg, error_msg);
                syslog_client_send_with_header(buffer, -1);
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(params->uart_port);
                xQueueReset(uart1_queue);
                break;
            //Event of UART RX break detected
            case UART_BREAK:
                if (last_event_type != UART_BREAK)
                {
                    error_msg = "[uart rx break]";
                    ESP_LOGW(TAG, "%s", error_msg);
                    strcpy(msg, error_msg);
                    syslog_client_send_with_header(buffer, -1);
                }
                break;
            //Event of UART frame error
            case UART_FRAME_ERR:
                if (last_event_type != UART_FRAME_ERR)
                {
                    error_msg = "[uart frame error]";
                    ESP_LOGW(TAG, "%s", error_msg);
                    strcpy(msg, error_msg);
                    syslog_client_send_with_header(buffer, -1);
                }
                break;
            //Others
            default:
                //ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
            last_event_type = event.type;
        }
    }
    free(buffer);
    buffer = NULL;
    vTaskDelete(NULL);
}

void configure_uart(uart_port_t uart_port, int baud_rate, int rx_io_num, const char *syslog_task_name)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */

    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    QueueHandle_t uart_queue;
    // install UART driver, and get the queue.
    uart_driver_install(uart_port, UART_BUF_SIZE, 0, UART_QUEUE_SIZE, &uart_queue, ESP_INTR_FLAG_IRAM);
    uart_param_config(uart_port, &uart_config);

    // change receive FIFO "full" threshold from 0x60 (default) to 0x3e
    uart_set_rx_full_threshold(uart_port, UART_RXFIFO_FULL_THRHD_V / 2);

    // set UART pins
    uart_set_pin(uart_port, UART_PIN_NO_CHANGE,
                            rx_io_num,
                            UART_PIN_NO_CHANGE,
                            UART_PIN_NO_CHANGE);

    // configure UART pattern detect function
    uart_enable_pattern_det_baud_intr(uart_port, PATTERN_CHR, PATTERN_CHR_NUM, 9, 0, 0);
    // reset the pattern queue length to record at most 500 pattern positions
    uart_pattern_queue_reset(uart_port, 500);

    // create a task to handler UART event from ISR
    task_params_t *params = (task_params_t *)malloc(sizeof(task_params_t));
    params->uart_port = uart_port;
    params->uart_queue = uart_queue;
    params->syslog_task_name = syslog_task_name;
    char *pcname = "uartX_event_task";
    pcname[4] = '0' + uart_port;
    xTaskCreate(uart_event_task, pcname, 2048, (void *)params, 12, NULL);
}

void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    /* initialize NVS — it is used to store PHY calibration data */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    bool connected = wifi_start(CONFIG_OWN_HOSTNAME, 5000);
    if (!connected)
    {
        ESP_LOGW(TAG, "Cannot connect to Wifi -> rebooting");
        esp_restart();
    }

    syslog_client_start(CONFIG_SYSLOG_HOST, CONFIG_SYSLOG_PORT, SYSLOG_LOCAL0);

#ifdef CONFIG_SYSLOG_USE_UART1
    configure_uart(UART_NUM_1,
                   CONFIG_SYSLOG_UART1_BAUD_RATE,
                   CONFIG_SYSLOG_UART1_RX_PIN,
                   CONFIG_SYSLOG_UART1_TASK_NAME);
#endif

#ifdef CONFIG_SYSLOG_USE_UART2
    configure_uart(UART_NUM_2,
                   CONFIG_SYSLOG_UART2_BAUD_RATE,
                   CONFIG_SYSLOG_UART2_RX_PIN,
                   CONFIG_SYSLOG_UART2_TASK_NAME);
#endif
}
