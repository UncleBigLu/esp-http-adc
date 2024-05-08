#include "http_adc_server.h"

#include <protocol_examples_utils.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "adcutils.h"

#define HTTP_QUERY_KEY_MAX_LEN 64

const char* TAG = "http_adc_server";
const char* query_param = "sample_num";


struct adc_sBuf_container
{
    adc_continuous_handle_t adc_handle;
    StreamBufferHandle_t sbuf_handle;
    bool should_adc_stop;
    SemaphoreHandle_t mutex_handle;
};

adc_sbuf_handle_t init_adc_sbuf()
{
    adc_sbuf_handle_t ret = malloc(sizeof(struct  adc_sBuf_container));
    if(ret == NULL)
        return NULL;
    ret->adc_handle = NULL;
    ret->sbuf_handle = NULL;
    ret->should_adc_stop = false;
    ret->mutex_handle = NULL;
    return ret;
}

void set_sbuf_handle(adc_sbuf_handle_t handle, StreamBufferHandle_t sbuf_handle)
{
    handle->sbuf_handle = sbuf_handle;
}

void set_adc_handle(adc_sbuf_handle_t handle, adc_continuous_handle_t adc_handle)
{
    handle->adc_handle = adc_handle;
}



void set_adc_stop_flag(adc_sbuf_handle_t handle, bool flag)
{
    handle->should_adc_stop = flag;
}

bool get_adc_stop_flag(adc_sbuf_handle_t handle)
{
    return handle->should_adc_stop;
}

void set_mutex(adc_sbuf_handle_t handle, SemaphoreHandle_t mutex_handle)
{
    handle->mutex_handle = mutex_handle;
}

SemaphoreHandle_t get_mutex(adc_sbuf_handle_t handle)
{
    return handle->mutex_handle;
}

adc_continuous_handle_t get_adc_handle(const adc_sbuf_handle_t handle)
{
    if(handle == NULL)
        return NULL;
    return handle->adc_handle;
}

StreamBufferHandle_t get_sbuf_handle(const adc_sbuf_handle_t handle)
{
    if(handle == NULL)
        return NULL;
    return handle->sbuf_handle;
}

void free_adc_sbuf(adc_sbuf_handle_t handle)
{
    free(handle);
}

static esp_err_t download_get_handler(httpd_req_t* req)
{
    adc_sbuf_handle_t adc_sbuf_handle = req->user_ctx;
    adc_continuous_handle_t adc_handle = get_adc_handle(adc_sbuf_handle);
    StreamBufferHandle_t sbuf_handle = get_sbuf_handle(adc_sbuf_handle);

    size_t buf_len = httpd_req_get_url_query_len(req) + 1;

    size_t adc_sample_num = 0;
    if(buf_len > 1)
    {
        char* buf = malloc(buf_len);
        if(httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[HTTP_QUERY_KEY_MAX_LEN], dec_param[HTTP_QUERY_KEY_MAX_LEN];
            if(httpd_query_key_value(buf, query_param, param, HTTP_QUERY_KEY_MAX_LEN) == ESP_OK)
            {
                example_uri_decode(dec_param, param, strnlen(param, HTTP_QUERY_KEY_MAX_LEN));
                adc_sample_num = atoi(dec_param);
                ESP_LOGI(TAG, "Transfer %d sample to host", adc_sample_num);

                /* Config http response header */
                /* As we are transfering binary u16 data.. */
                ESP_ERROR_CHECK(httpd_resp_set_type(req, "application/octet-stream"));

                /* Send ADC data back */
                /* Start ADC */
                ESP_LOGI(TAG, "Start ADC sampling");
                ESP_ERROR_CHECK(adc_continuous_start(adc_handle));

                // Each trunk contains sampled data from one ADC conversion
                const size_t adc_conv_data_num = ADC_CONV_FRAME_SZ / SOC_ADC_DIGI_RESULT_BYTES;
                ESP_LOGI(TAG, "buflen: %u", adc_conv_data_num);
                uint16_t chunk_buf[adc_conv_data_num];

                while (adc_sample_num > 0)
                {
                    for (size_t chunk_buf_ptr = 0; chunk_buf_ptr < adc_conv_data_num; ++chunk_buf_ptr)
                    {

                        /* Will block here if no data available */
                        xStreamBufferReceive(sbuf_handle, (void*)&(chunk_buf[chunk_buf_ptr]),
                                             sizeof(uint16_t), portMAX_DELAY);
                    }

                    // Send http response
                    size_t chunk_size = adc_conv_data_num < adc_sample_num
                                            ? adc_conv_data_num * sizeof(uint16_t)
                                            : adc_sample_num * sizeof(uint16_t);
                    if (httpd_resp_send_chunk(req, (char*)chunk_buf, chunk_size) != ESP_OK)
                    {
                        ESP_LOGE(TAG, "File sending failed!");
                        /* Abort sending file */
                        httpd_resp_sendstr_chunk(req, NULL);
                        /* Respond with 500 Internal Server Error */
                        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                        ESP_ERROR_CHECK(adc_continuous_stop(adc_handle));
                        break;
                    }
                    adc_sample_num -= chunk_size / sizeof(uint16_t);
                }

                // Let the ADC task to stop the ADC and flush ADC buffer
                SemaphoreHandle_t mutex_handle = get_mutex(adc_sbuf_handle);
                if(xSemaphoreTake(mutex_handle, portMAX_DELAY) == pdTRUE)
                {
                    ESP_LOGI(TAG, "Set stop flag to true");
                    set_adc_stop_flag(adc_sbuf_handle, true);
                    xSemaphoreGive(mutex_handle);
                }

            }
        }
        free(buf);
    }
    return ESP_OK;
}

esp_err_t start_server(adc_sbuf_handle_t adc_sbuf_handle)
{
    httpd_handle_t httpd_handle = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    //config.uri_match_fn = httpd_uri_match_wildcard;
    ESP_LOGI(TAG, "Starting HTTP Server on port %d", config.server_port);

    if(httpd_start(&httpd_handle, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start server");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Server start success");
    httpd_uri_t adc_sample_download = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = download_get_handler,
        .user_ctx = adc_sbuf_handle
    };
    httpd_register_uri_handler(httpd_handle, &adc_sample_download);

    return ESP_OK;
}