#ifndef _HTTP_ADC_SERVER_H_
#define _HTTP_ADC_SERVER_H_

#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/semphr.h"


struct adc_sBuf_container;

typedef struct adc_sBuf_container* adc_sbuf_handle_t;

adc_sbuf_handle_t init_adc_sbuf();
void free_adc_sbuf(adc_sbuf_handle_t);
void set_adc_handle(adc_sbuf_handle_t handle, adc_continuous_handle_t adc_handle);
void set_sbuf_handle(adc_sbuf_handle_t handle, StreamBufferHandle_t sbuf_handle);
adc_continuous_handle_t get_adc_handle(adc_sbuf_handle_t);
StreamBufferHandle_t get_sbuf_handle(adc_sbuf_handle_t);
void set_adc_stop_flag(adc_sbuf_handle_t handle, bool flag);
bool get_adc_stop_flag(adc_sbuf_handle_t handle);
void set_mutex(adc_sbuf_handle_t handle, SemaphoreHandle_t mutex_handle);
SemaphoreHandle_t get_mutex(adc_sbuf_handle_t handle);
void set_adc_start_flag(adc_sbuf_handle_t handle, bool flag);
bool get_adc_start_flag(adc_sbuf_handle_t handle);
void set_task_handle(adc_sbuf_handle_t handle, TaskHandle_t task_handle);
TaskHandle_t get_task_handle(adc_sbuf_handle_t handle);
void set_binSemaphore(adc_sbuf_handle_t handle, SemaphoreHandle_t semaphore_handle);
SemaphoreHandle_t get_binSemaphore_handle(adc_sbuf_handle_t handle);

esp_err_t start_server(adc_sbuf_handle_t);

#endif
