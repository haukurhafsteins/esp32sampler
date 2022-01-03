#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/message_buffer.h"
#include "driver/i2s.h"
#include "driver/adc.h"
#include "soc/syscon_reg.h"
#include "soc/syscon_periph.h"
#include "esp_log.h"
#include "i2sadc.h"
#include "time_manager.h"

#define SAMPLE_RATE (25000)
#define I2S_NUM (0)
#define CHANNEL_NUM (8)
#define DMA_BUFCNT (2)
#define DMA_BUFSIZE (1024)

typedef struct
{
    event_receiver_t receiver;
    uint32_t sample_time_us;
} request_scan_t;

typedef struct
{
    event_receiver_t receiver;
    uint32_t sample_rate;
} request_set_sample_freq_t;

typedef enum
{
    sampling_state_idle,
    sampling_state_preparing,
    sampling_state_sampling
} sampling_state_t;

static QueueHandle_t s_i2s_event_queue;
static const char *TAG = "I2S";
static uint32_t sample_rate = SAMPLE_RATE;
static esp_event_base_t I2SADC_BASE = "I2SADC_BASE";
static esp_event_loop_handle_t loop_handle;
static sampling_state_t sampling_state = sampling_state_idle;
static int64_t start_time;
static request_scan_t request_scan;
static size_t requested_samples = 500;
static size_t sample_counter;
static size_t total_bytes_read;
static uint16_t i2s_read_buff[1024];
static i2sadc_samples_t *adc_samples;
static size_t adc_sample_size;
static TaskHandle_t i2s_scanner_task_handle;
static uint32_t task_wait_time = 5;

static size_t get_nr_samples(uint32_t sample_time_us)
{
    return (sample_rate * sample_time_us) / 1000000;
}

static float calibrate_adc(uint8_t chan, uint16_t value)
{
    // y = ax + b
    return 0.0008100147275 * value + 0.1519145803;
}

void i2s_adc_init()
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN,
        .sample_rate = sample_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .dma_buf_count = DMA_BUFCNT, // dma_desc_num
        .dma_buf_len = DMA_BUFSIZE,  // dma_frame_num
        .use_apll = 1,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1
    };

    i2s_driver_install(I2S_NUM, &i2s_config, 2, &s_i2s_event_queue);
    i2s_set_adc_mode(I2S_NUM, ADC1_CHANNEL_0);
    i2s_adc_enable(I2S_NUM);
    vTaskDelay(pdMS_TO_TICKS(5000));

    // This 32 bit register has 4 bytes for the first set of channels to scan.
    // Each byte consists of:
    // [7:4] Channel
    // [3:2] Bit Width; 3=12bit, 2=11bit, 1=10bit, 0=9bit
    // [1:0] Attenuation; 3=11dB, 2=6dB, 1=2.5dB, 0=0dB
    WRITE_PERI_REG(SYSCON_SARADC_SAR1_PATT_TAB1_REG, 0x0F1F2F3F);
    WRITE_PERI_REG(SYSCON_SARADC_SAR1_PATT_TAB2_REG, 0x4F5F6F7F);

    // Scan multiple channels.
    SET_PERI_REG_BITS(SYSCON_SARADC_CTRL_REG, SYSCON_SARADC_SAR1_PATT_LEN, 7, SYSCON_SARADC_SAR1_PATT_LEN_S);

    // The raw ADC data is written to DMA in inverted form. Invert back.
    SET_PERI_REG_MASK(SYSCON_SARADC_CTRL2_REG, SYSCON_SARADC_SAR1_INV);


}

static void i2s_set_sample_freq(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    request_set_sample_freq_t *request_set_sample_freq = (request_set_sample_freq_t *)event_data;
    sample_rate = request_set_sample_freq->sample_rate;
    ESP_ERROR_CHECK(i2s_set_sample_rates(I2S_NUM, sample_rate));
}

static void i2s_read_adc()
{
    size_t bytes_read = 0;
    ESP_ERROR_CHECK(i2s_read(I2S_NUM, (void *)i2s_read_buff, sizeof(i2s_read_buff), &bytes_read, portMAX_DELAY));
    for (int i = 0; i < bytes_read / 2; i++)
    {
        if (sample_counter + i / CHANNEL_NUM >= requested_samples)
            break;
        uint8_t chan = (i2s_read_buff[i] >> 12) & 0x07;
        uint16_t adc_value = i2s_read_buff[i] & 0xfff;
        adc_samples->samples[sample_counter + i / CHANNEL_NUM].sample[chan] = calibrate_adc(chan, adc_value);
    }
    total_bytes_read += bytes_read;
    sample_counter = total_bytes_read / (CHANNEL_NUM * sizeof(uint16_t));
    if (requested_samples > sample_counter)
        return;

    int64_t end = esp_timer_get_time() - start_time;
    adc_samples->nr_samples = requested_samples;

    ESP_LOGW(TAG, "i2s_scan: end, time=%lldus, total 8ch samples=%d", end, adc_samples->nr_samples);

    esp_event_post_to(request_scan.receiver.evloop, request_scan.receiver.base, request_scan.receiver.id, (void *)adc_samples, adc_sample_size, (TickType_t)0);

    i2s_free_samples(adc_samples);
    adc_samples = NULL;
    sampling_state = sampling_state_idle;
}

static void i2s_scanner(void *arg)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(task_wait_time));

        if (sampling_state == sampling_state_idle)
        {
            size_t bytes_read = 0;
            ESP_ERROR_CHECK(i2s_read(I2S_NUM, (void *)i2s_read_buff, sizeof(i2s_read_buff), &bytes_read, portMAX_DELAY));
            continue;
        }
        else if (sampling_state == sampling_state_preparing)
        {
            sample_counter = 0;
            total_bytes_read = 0;
            sampling_state = sampling_state_sampling;
            start_time = esp_timer_get_time();
        }
        i2s_read_adc();
    }
}
static void i2s_request_scan(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (sampling_state != sampling_state_idle)
        return;

    request_scan = *((request_scan_t *)event_data);
    requested_samples = get_nr_samples(request_scan.sample_time_us);
    if (requested_samples == 0)
    {
        ESP_LOGE(TAG, "requested_samples is zero");
        return;
    }

    ESP_LOGI(TAG, "i2s_scan: start, sample_time_us=%dus, requested_samples=%d", request_scan.sample_time_us, requested_samples);
    adc_sample_size = sizeof(i2sadc_samples_t) + sizeof(i2sadc_sample_t) * requested_samples;
    adc_samples = (i2sadc_samples_t *)calloc(adc_sample_size, sizeof(char));
    if (adc_samples == NULL)
    {
        ESP_LOGE(TAG, "adc_samples allocation failed");
        return;
    }

    sampling_state = sampling_state_preparing;
}

bool i2sadc_post_sample_request(event_receiver_t *receiver, uint32_t sample_time_us)
{
    request_scan_t ev_data = {*receiver, sample_time_us};
    return (ESP_OK == esp_event_post_to(loop_handle, I2SADC_BASE, I2SADC_SCAN, (void *)&ev_data, sizeof(ev_data), portMAX_DELAY));
}

bool i2sadc_post_set_sample_frequency(event_receiver_t *receiver, uint32_t new_sample_freq)
{
    request_set_sample_freq_t ev_data = {*receiver, new_sample_freq};
    return (ESP_OK == esp_event_post_to(loop_handle, I2SADC_BASE, I2SADC_SET_SAMPLE_FREQ, (void *)&ev_data, sizeof(ev_data), portMAX_DELAY));
}

void i2s_free_samples(i2sadc_samples_t *adc_samples)
{
    free(adc_samples);
    adc_samples = NULL;
}

void i2s_setup_eventloop()
{
    ESP_EVENT_DECLARE_BASE(I2SADC_BASE);
    esp_event_loop_args_t loop_args = {
        .queue_size = 10,
        .task_name = "i2s_scanner_task",
        .task_priority = 0,
        .task_stack_size = 2048 * 2,
        .task_core_id = 1};
    ESP_ERROR_CHECK(esp_event_loop_create(&loop_args, &loop_handle));
    ESP_ERROR_CHECK(esp_event_handler_register_with(loop_handle, I2SADC_BASE, I2SADC_SCAN, &i2s_request_scan, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register_with(loop_handle, I2SADC_BASE, I2SADC_SET_SAMPLE_FREQ, &i2s_set_sample_freq, NULL));
}

void i2s_init()
{
    i2s_setup_eventloop();
    i2s_adc_init();
    xTaskCreate(i2s_scanner, "i2s_scanner", 2048, NULL, 5, &i2s_scanner_task_handle);
}