#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"
#include "myutils.h"

#define EXAMPLE_ADC_UNIT                    ADC_UNIT_1
#define _EXAMPLE_ADC_UNIT_STR(unit)         #unit
#define EXAMPLE_ADC_UNIT_STR(unit)          _EXAMPLE_ADC_UNIT_STR(unit)
#define EXAMPLE_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
#define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_0
#define EXAMPLE_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type1.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type1.data)
#else
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#endif

#define EXAMPLE_READ_LEN                    256
#define SAMPLE_RATE  20000

#if CONFIG_IDF_TARGET_ESP32
static adc_channel_t channel[2] = {ADC_CHANNEL_6, ADC_CHANNEL_7};
#else
// As we're using ESP32S3...
// Only one channel is required
// Should map to GPIO 3
static adc_channel_t channel[1] = {ADC_CHANNEL_2};
#endif

static TaskHandle_t s_task_handle;
static const char *TAG = "freq_sample";

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = SAMPLE_RATE,
        .conv_mode = EXAMPLE_ADC_CONV_MODE,
        .format = EXAMPLE_ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;
        adc_pattern[i].channel = channel[i] & 0x7;
        adc_pattern[i].unit = EXAMPLE_ADC_UNIT;
        adc_pattern[i].bit_width = EXAMPLE_ADC_BIT_WIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
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
    ESP_LOGI(TAG,"Sample frequency: %d kHz, data per frame: %d\n", SAMPLE_RATE, EXAMPLE_READ_LEN/SOC_ADC_DIGI_RESULT_BYTES);
    // Start Sampling
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    esp_err_t ret;
    uint8_t result[EXAMPLE_READ_LEN] = {0};
    uint32_t ret_num = 0;
    char unit[] = EXAMPLE_ADC_UNIT_STR(EXAMPLE_ADC_UNIT);

    double high_val_avg = 0;
    double low_val_avg = 0;
    uint32_t data_cnt = 0, high_data_cnt = 0, low_data_cnt = 0;

    struct SlidingWindow s = initWindow();

    while(1){
        // Block here until data available
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Read all data out
        while(1) {
            ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
            if(ret == ESP_ERR_TIMEOUT){
                // No data available in the buffer
                break;
            } else if(ret == ESP_OK){
                for(int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES){
                    adc_digi_output_data_t *p = (adc_digi_output_data_t*)&result[i];
                    uint32_t chan_num = EXAMPLE_ADC_GET_CHANNEL(p);
                    uint32_t data = EXAMPLE_ADC_GET_DATA(p);

                    if (chan_num < SOC_ADC_CHANNEL_NUM(EXAMPLE_ADC_UNIT)){
                        push(&s, data);
                        if(isFull(s))
                        {
                            if(data > s.avg)
                            {
                                high_val_avg += (double)data;
                                ++high_data_cnt;
                            }else
                            {
                                low_val_avg += (double)data;
                                ++low_data_cnt;
                            }
                            ++data_cnt;
                        }
                    } else {
                        ESP_LOGW(TAG, "Invalid data [%s_%"PRIu32"_%"PRIx32"]", unit, chan_num, data);
                    }

                }
                // Data process algorithm here
                if(data_cnt >= SAMPLE_RATE)
                {
                    high_val_avg /= high_data_cnt;
                    low_val_avg /= low_data_cnt;
                    ESP_LOGI(TAG, "Avg power: %f\tHigh level: %f\tLow level: %f\t"
                                  "Frequency: wip\tDuty cycle: %f",
                                  s.avg, high_val_avg, low_val_avg, (double)high_data_cnt/low_data_cnt);
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


