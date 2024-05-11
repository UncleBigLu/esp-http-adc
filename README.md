Send adc sample to host through http

Tested on **ESP32S3** board.

## Usage:

`curl http_sample.lan/?sample_num=<required sample num> > sample.bin`

**Warning**: Concurrency GET requests is **NOT** supported(i.e, only one GET request should be sent at a time) as we use
stream buffer to transmit data between tasks.

## ADC related configuration

| Variable          | Description                                                | value             |
|-------------------|------------------------------------------------------------|-------------------|
| adc_channel_t     | ADC channel.                                               | GPIO 3 on ESP32S3 |
| ADC_SAMPLE_RATE   | sample rate                                                | 20000 Hz          |
| EXAMPLE_ADC_ATTEN | It seems dB0=>**0~1.1v**, but further test maybe required. | ADC_ATTEN_DB_0    |

## Http related configuration

| Variable            | Description            | value       |
|---------------------|------------------------|-------------|
| query_param         | http request parameter | sample_num  |
| LWIP_LOCAL_HOSTNAME | hostname (in kconfig)  | http_sample |
