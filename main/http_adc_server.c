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
    bool should_adc_start;
    SemaphoreHandle_t mutex_handle;
    TaskHandle_t adc_task_handle;
    SemaphoreHandle_t binSemaphore_handle;
};

adc_sbuf_handle_t init_adc_sbuf()
{
    adc_sbuf_handle_t ret = malloc(sizeof(struct adc_sBuf_container));
    if (ret == NULL)
        return NULL;
    ret->adc_handle = NULL;
    ret->sbuf_handle = NULL;
    ret->should_adc_stop = false;
    ret->mutex_handle = NULL;
    ret->should_adc_start = false;
    ret->adc_task_handle = NULL;
    ret->binSemaphore_handle = NULL;
    return ret;
}

void set_binSemaphore(adc_sbuf_handle_t handle, SemaphoreHandle_t semaphore_handle)
{
    handle->binSemaphore_handle = semaphore_handle;
}

SemaphoreHandle_t get_binSemaphore_handle(adc_sbuf_handle_t handle)
{
    return handle->binSemaphore_handle;
}

void set_task_handle(adc_sbuf_handle_t handle, TaskHandle_t task_handle)
{
    handle->adc_task_handle = task_handle;
}

TaskHandle_t get_task_handle(adc_sbuf_handle_t handle)
{
    return handle->adc_task_handle;
}

void set_sbuf_handle(adc_sbuf_handle_t handle, StreamBufferHandle_t sbuf_handle)
{
    handle->sbuf_handle = sbuf_handle;
}

void set_adc_handle(adc_sbuf_handle_t handle, adc_continuous_handle_t adc_handle)
{
    handle->adc_handle = adc_handle;
}

void set_adc_start_flag(adc_sbuf_handle_t handle, bool flag)
{
    handle->should_adc_start = flag;
}

bool get_adc_start_flag(adc_sbuf_handle_t handle)
{
    return handle->should_adc_start;
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
    if (handle == NULL)
        return NULL;
    return handle->adc_handle;
}

StreamBufferHandle_t get_sbuf_handle(const adc_sbuf_handle_t handle)
{
    if (handle == NULL)
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
    if (adc_sbuf_handle == NULL)
    {
        ESP_LOGE(TAG, "adc_sbuf_handle is NULL");
        return ESP_FAIL;
    }
    StreamBufferHandle_t sbuf_handle = get_sbuf_handle(adc_sbuf_handle);
    if (sbuf_handle == NULL)
    {
        ESP_LOGE(TAG, "NULL sbuf handle");
        return ESP_FAIL;
    }
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;

    size_t adc_sample_num = 0;
    if (buf_len > 1)
    {
        char* buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[HTTP_QUERY_KEY_MAX_LEN], dec_param[HTTP_QUERY_KEY_MAX_LEN];
            if (httpd_query_key_value(buf, query_param, param, HTTP_QUERY_KEY_MAX_LEN) == ESP_OK)
            {
                example_uri_decode(dec_param, param, strnlen(param, HTTP_QUERY_KEY_MAX_LEN));
                adc_sample_num = atoi(dec_param);
                ESP_LOGI(TAG, "Transfer %d sample to host", adc_sample_num);

                /* Config http response header */
                /* As we are transfering binary u16 data.. */
                ESP_ERROR_CHECK(httpd_resp_set_type(req, "application/octet-stream"));

                /* Send ADC data back */

                // Each trunk contains sampled data from one ADC conversion
                const size_t adc_conv_data_num = ADC_CONV_FRAME_SZ / SOC_ADC_DIGI_RESULT_BYTES;
                ESP_LOGI(TAG, "buflen: %u", adc_conv_data_num);
                uint16_t chunk_buf[adc_conv_data_num];

                /* If the stream buffer is full, flush the buffer to discard old data */
                if(xStreamBufferIsFull(sbuf_handle) == pdTRUE)
                {
                    ESP_LOGI(TAG, "Flush stream buffer");
                    size_t bytes_to_recv = xStreamBufferBytesAvailable(sbuf_handle);
                    while(bytes_to_recv > 0)
                    {
                        bytes_to_recv -= xStreamBufferReceive(sbuf_handle, chunk_buf,
                            sizeof(uint16_t), 0);
                    }

                }

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

                        break;
                    }
                    adc_sample_num -= chunk_size / sizeof(uint16_t);
                    if(adc_sample_num <= 0)
                    {
                        /* Send a chunk with zero-length to indicate end of transfer */
                        httpd_resp_send_chunk(req, NULL, 0);
                    }
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

    if (httpd_start(&httpd_handle, &config) != ESP_OK)
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
