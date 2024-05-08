#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"
#include "myutils.h"
#include "esp_dsp.h"
#include "adcutils.h"
#include "http_adc_server.h"
#include "freertos/stream_buffer.h"

static TaskHandle_t s_task_handle;
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

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t* edata,
                                     void* user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}


void app_main(void)
{
    s_task_handle = xTaskGetCurrentTaskHandle();

    // Regist callback
    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));

    // Print configration info
    ESP_LOGI(TAG, "Sample frequency: %d kHz, data per frame: %d\n", ADC_SAMPLE_RATE,
             ADC_CONV_FRAME_SZ/SOC_ADC_DIGI_RESULT_BYTES);
    // Start Sampling
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    esp_err_t ret;
    uint8_t result[ADC_CONV_FRAME_SZ] = {0};
    uint32_t ret_num = 0;
    char unit[] = EXAMPLE_ADC_UNIT_STR(EXAMPLE_ADC_UNIT);

    double high_val_avg = 0;
    double low_val_avg = 0;
    uint32_t data_cnt = 0, high_data_cnt = 0, low_data_cnt = 0;

    slidingWindowHandler swindow_handler = initWindow(WINDOW_SIZE);

    while (1)
    {
        // Block here until data available
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Read all data out
        while (1)
        {
            ret = adc_continuous_read(handle, result, ADC_CONV_FRAME_SZ, &ret_num, 0);
            if (ret == ESP_ERR_TIMEOUT)
            {
                // No data available in the buffer
                break;
            }
            else if (ret == ESP_OK)
            {
                for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES)
                {
                    adc_digi_output_data_t* p = (adc_digi_output_data_t*)&result[i];
                    uint32_t chan_num = EXAMPLE_ADC_GET_CHANNEL(p);
                    uint32_t data = EXAMPLE_ADC_GET_DATA(p);

                    if (chan_num < SOC_ADC_CHANNEL_NUM(EXAMPLE_ADC_UNIT))
                    {
                        push(swindow_handler, data);
                        if (isFull(swindow_handler))
                        {
                            if (data > getAvg(swindow_handler))
                            {
                                high_val_avg += (double)data;
                                ++high_data_cnt;
                            }
                            else
                            {
                                low_val_avg += (double)data;
                                ++low_data_cnt;
                            }
                            ++data_cnt;
                        }
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Invalid data [%s_%"PRIu32"_%"PRIx32"]", unit, chan_num, data);
                    }
                }
                // Data process algorithm here
                if (data_cnt >= ADC_SAMPLE_RATE)
                {
                    high_val_avg /= high_data_cnt;
                    low_val_avg /= low_data_cnt;
                    ESP_LOGI(TAG, "Avg power: %f\tHigh level: %f\tLow level: %f\t"
                             "Frequency: wip\tDuty cycle: %f",
                             getAvg(swindow_handler), high_val_avg, low_val_avg, (double)high_data_cnt/low_data_cnt);
                    high_val_avg = 0;
                    low_val_avg = 0;
                    high_data_cnt = 0;
                    low_data_cnt = 0;
                    data_cnt = 0;
                }
            }
        }
    }
}
