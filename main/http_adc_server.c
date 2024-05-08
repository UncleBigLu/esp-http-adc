#include "http_adc_server.h"
#include "esp_http_server.h"
#include "esp_log.h"

const char* TAG = "http_adc_server";

struct adc_sBuf_container
{
    adc_continuous_handle_t adc_handle;
    StreamBufferHandle_t sbuf_handle;
};

adc_sbuf_handle_t init_adc_sbuf(adc_continuous_handle_t adc_handle, StreamBufferHandle_t sbuf_handle)
{
    adc_sbuf_handle_t ret = malloc(sizeof(struct  adc_sBuf_container));
    if(ret == NULL)
        return NULL;
    ret->adc_handle = adc_handle;
    ret->sbuf_handle = sbuf_handle;
    return ret;
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
    return ESP_OK;
}

esp_err_t start_server(adc_sbuf_handle_t adc_sbuf_handle)
{
    httpd_handle_t httpd_handle = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    ESP_LOGI(TAG, "Starting HTTP Server on port %d", config.server_port);

    if(httpd_start(&httpd_handle, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start server");
        return ESP_FAIL;
    }

    httpd_uri_t adc_sample_download = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = download_get_handler,
        .user_ctx = adc_sbuf_handle
    };
    httpd_register_uri_handler(httpd_handle, &adc_sample_download);

    return ESP_OK;
}