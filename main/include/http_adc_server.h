#ifndef _HTTP_ADC_SERVER_H_
#define _HTTP_ADC_SERVER_H_

#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"



struct adc_sBuf_container;

typedef struct adc_sBuf_container* adc_sbuf_handle_t;

adc_sbuf_handle_t init_adc_sbuf(adc_continuous_handle_t, StreamBufferHandle_t);
void free_adc_sbuf(adc_sbuf_handle_t);
adc_continuous_handle_t get_adc_handle(adc_sbuf_handle_t);
StreamBufferHandle_t get_sbuf_handle(adc_sbuf_handle_t);

esp_err_t start_server(adc_sbuf_handle_t);

#endif
