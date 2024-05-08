#include <nvs_flash.h>
#include <protocol_examples_common.h>
#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"
#include "esp_event.h"
#include "myutils.h"
#include "adcutils.h"
#include "http_adc_server.h"
#include "freertos/stream_buffer.h"
#include <esp_http_server.h>


static const char* TAG = "freq_sample";

#if CONFIG_IDF_TARGET_ESP32
static adc_channel_t channel[2] = {ADC_CHANNEL_6, ADC_CHANNEL_7};
#else
// As we're using ESP32S3...
// Only one channel is required
// Should map to GPIO 3
static adc_channel_t channel[1] = {ADC_CHANNEL_2};
#endif

/* Size of sliding window which stores data from ADC */
#define WINDOW_SIZE 128


void app_main(void)
{
    /* Helper container to transmit EVERYTHING required into different tasks.. */
    adc_sbuf_handle_t adc_sbuf_handle = NULL;
    adc_sbuf_handle = init_adc_sbuf();
    if (adc_sbuf_handle == NULL)
    {
        ESP_LOGE(TAG, "No memory for adc_sbuf_handle");
        return;
    }

    /* ADC initial */
    adc_continuous_handle_t adc_handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &adc_handle);
    set_adc_handle(adc_sbuf_handle, adc_handle);

    /* Stream buffer */
    StreamBufferHandle_t sbuf_handle = xStreamBufferCreate(
        2*ADC_CONV_FRAME_SZ/SOC_ADC_DIGI_RESULT_BYTES*sizeof(uint16_t),
        sizeof(uint16_t)
    );
    set_sbuf_handle(adc_sbuf_handle, sbuf_handle);

    /* Mutex to control ADC stop */
    SemaphoreHandle_t mutex_handle = xSemaphoreCreateMutex();
    set_mutex(adc_sbuf_handle, mutex_handle);


    /* Create ADC task
     * Note that the task will block itself until httpd start adc sampling
     *
     * Initial adc_sbuf_handle first before pass it to other task
     */
    TaskHandle_t adc_task_handle = NULL;
    if (xTaskCreate(adc_sample_task, "adc_sample_task",
                    2048, adc_sbuf_handle, 1, adc_task_handle) != pdPASS)
    {
        ESP_LOGE(TAG, "ADC task creation failed");
        return;
    }
    /* Register conversion done callback */
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, adc_task_handle));

    // Print configration info
    ESP_LOGI(TAG, "Sample frequency: %d kHz, data per frame: %d\n", ADC_SAMPLE_RATE,
             ADC_CONV_FRAME_SZ/SOC_ADC_DIGI_RESULT_BYTES);


    /* Initial wifi */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    ESP_ERROR_CHECK(start_server(adc_sbuf_handle));

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
