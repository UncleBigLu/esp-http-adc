#include "adcutils.h"

#include <http_adc_server.h>

#include "esp_log.h"


static const char* TAG = "adcutils";

bool IRAM_ATTR conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t* edata,
                                     void* user_data)
{
    TaskHandle_t task_handle = (TaskHandle_t)user_data;
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

void continuous_adc_init(adc_channel_t* channel, uint8_t channel_num, adc_continuous_handle_t* out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = ADC_CONV_FRAME_SZ,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = ADC_SAMPLE_RATE,
        .conv_mode = EXAMPLE_ADC_CONV_MODE,
        .format = EXAMPLE_ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++)
    {
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

void adc_sample_task(void* parameter)
{
    adc_sbuf_handle_t adc_sbuf_handle = (adc_sbuf_handle_t)parameter;

    adc_continuous_handle_t adc_handle = get_adc_handle(adc_sbuf_handle);
    if(adc_handle == NULL)
    {
        ESP_LOGE(TAG, "NULL adc handle");
        return;
    }
    uint8_t sample_buf[ADC_CONV_FRAME_SZ];
    StreamBufferHandle_t sbuf_handle = get_sbuf_handle(adc_sbuf_handle);
    if(sbuf_handle == NULL)
    {
        ESP_LOGE(TAG, "NULL stream buffer handle");
        return;
    }
    SemaphoreHandle_t mutex_handle = get_mutex(adc_sbuf_handle);
    if(mutex_handle == NULL)
    {
        ESP_LOGE(TAG, "NULL mutex handle");
        return;
    }

    while (1)
    {
        /* Block here if no data available */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint32_t out_bytes;
        esp_err_t ret = adc_continuous_read(adc_handle, sample_buf, ADC_CONV_FRAME_SZ, &out_bytes, 0);

        if (ret == ESP_ERR_TIMEOUT)
        {
            // No data available in the buffer, continue to self block and wait for new data
            continue;
        }
        if (ret == ESP_ERR_INVALID_STATE)
        {
            ESP_LOGW(TAG, "ADC internal pool full. Check ADC sample rate");
        }
        if (ret == ESP_OK)
        {
            /* Forward data to Stream buffer */
            for (int i = 0; i < out_bytes; i += SOC_ADC_DIGI_RESULT_BYTES)
            {
                adc_digi_output_data_t* p = (adc_digi_output_data_t*)&sample_buf[i];
                uint32_t chan_num = EXAMPLE_ADC_GET_CHANNEL(p);
                uint16_t data = EXAMPLE_ADC_GET_DATA(p);

                /* If the data is valid */
                if (chan_num < SOC_ADC_CHANNEL_NUM(EXAMPLE_ADC_UNIT))
                {
                    /* Forward data to Stream buffer */
                    if (xStreamBufferSpacesAvailable(sbuf_handle) < sizeof(data))
                    {
                        ESP_LOGW(TAG, "Stream buffer full. ADC sample rate may larger than Wifi through put");
                    }
                    size_t send_bytes = xStreamBufferSend(sbuf_handle, &data, sizeof(data), portMAX_DELAY);
                    if (send_bytes != sizeof(data))
                    {
                        ESP_LOGE(TAG, "Unexpected behavior.");
                    }
                }
                else
                {
                    ESP_LOGW(TAG, "Invalid data");
                }
            }

            /* Check if ADC should be stoped, which means server requires no more data */
            if(xSemaphoreTake(mutex_handle, portMAX_DELAY) == pdTRUE)
            {
                if(get_adc_stop_flag(adc_sbuf_handle))
                {
                    ESP_LOGI(TAG, "Stopping ADC");
                    ESP_ERROR_CHECK(adc_continuous_stop(adc_handle));
                    ESP_LOGI(TAG, "Flush ADC pool");
                    ESP_ERROR_CHECK(adc_continuous_flush_pool(adc_handle));
                    xSemaphoreGive(mutex_handle);
                    /* Self block until httpd enable adc again */
                    continue;
                }
                xSemaphoreGive(mutex_handle);
            }
        }
    }
}
