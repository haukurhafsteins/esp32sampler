
#include "esp_event.h"

#define ADC_BUFSIZE 1024
#define ADC_CHANNELS 8

enum i2sadc_events
{
    I2SADC_SCAN,
    I2SADC_SET_SAMPLE_FREQ,
    I2SADC_SET_SAMPLE_TIME,
    I2SADC_STAT
};

typedef struct
{
    esp_event_loop_handle_t evloop;
    esp_event_base_t base;
    int32_t id;
} event_receiver_t;

typedef struct
{
    float sample[ADC_CHANNELS];
} i2sadc_sample_t;

typedef struct
{
    size_t nr_samples;
    i2sadc_sample_t samples[0];
} i2sadc_samples_t;

#ifdef __cplusplus
extern "C"
{
#endif
    bool i2sadc_post_sample_request(event_receiver_t *receiver, uint32_t sample_time_us);
    bool i2sadc_post_set_sample_frequency(event_receiver_t *receiver, uint32_t sample_freq);
    void i2s_free_samples(i2sadc_samples_t *adc_samples);
#ifdef __cplusplus
}
#endif