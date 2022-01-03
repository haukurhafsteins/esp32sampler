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
    uint32_t sample_freq;
} request_set_sample_freq_t;

static QueueHandle_t s_i2s_event_queue;
static const char *TAG = "I2S";
static uint32_t sample_freq = 0;
static uint16_t channel_samples_counter[8];
static uint16_t loop_counter;
esp_event_base_t I2SADC_BASE = "I2SADC_BASE";
esp_event_loop_handle_t i2sadc_loop_handle;

//#define US_TO_SAMPLECOUNT(time_us) ((sample_freq * time_us) / 1000000)

static size_t get_nr_samples(uint32_t sample_time_us)
{
    return (sample_freq * sample_time_us) / 1000000;
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
        .sample_rate = sample_freq,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .dma_buf_count = DMA_BUFCNT, // dma_desc_num
        .dma_buf_len = DMA_BUFSIZE,  // dma_frame_num
        .use_apll = 1,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1};

    i2s_driver_install(I2S_NUM, &i2s_config, 2, &s_i2s_event_queue);
    i2s_set_adc_mode(I2S_NUM, ADC1_CHANNEL_0);

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

    // reduce sample time for 2Msps
    SYSCON.saradc_ctrl.sar_clk_div = 2;
    SYSCON.saradc_fsm.sample_cycle = 2;

    // sampling rate 2Msps setting
    I2S0.clkm_conf.clkm_div_num = 20;
    I2S0.clkm_conf.clkm_div_b = 0;
    I2S0.clkm_conf.clkm_div_a = 1;
    I2S0.sample_rate_conf.rx_bck_div_num = 2;
}

static void i2s_set_sample_freq(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    request_set_sample_freq_t *request_set_sample_freq = (request_set_sample_freq_t *)event_data;
    sample_freq = request_set_sample_freq->sample_freq;
    i2s_driver_uninstall(I2S_NUM);
    i2s_adc_init();
    //    esp_event_post_to(request_set_sample_freq->receiver.evloop, request_set_sample_freq->receiver.base, request_set_sample_freq->receiver.id, (void *)sample_freq, sizeof(sample_freq), (TickType_t)0);
}

static void i2s_scan(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    request_scan_t *request_scan = (request_scan_t *)event_data;
    size_t requested_samples = get_nr_samples(request_scan->sample_time_us);
    if (requested_samples == 0)
    {
        ESP_LOGE(TAG, "requested_samples is zero");
        return;
    }

    ESP_LOGI(TAG, "i2s_scan: start, sample_time_us=%dus, requested_samples=%d", request_scan->sample_time_us, requested_samples);

    size_t adc_sample_size = sizeof(i2sadc_samples_t) + sizeof(i2sadc_sample_t) * requested_samples;
    i2sadc_samples_t *adc_samples = (i2sadc_samples_t *)calloc(adc_sample_size, sizeof(char));
    if (adc_samples == NULL)
    {
        ESP_LOGE(TAG, "adc_samples allocation failed");
        return;
    }
    size_t i2s_read_buff_size = sizeof(uint16_t) * CHANNEL_NUM * requested_samples;
    uint16_t *i2s_read_buff = (uint16_t *)calloc(i2s_read_buff_size, sizeof(char));
    
    loop_counter = 0;
    for (int i = 0; i < 8; i++)
        channel_samples_counter[i] = 0;

    ESP_ERROR_CHECK(i2s_adc_enable(I2S_NUM));
    size_t bytes_read = 0;
    size_t sample_counter = 0;
    size_t total_bytes = 0;
    int64_t now = esp_timer_get_time();
    do
    {
        system_event_t evt;
        if (xQueueReceive(s_i2s_event_queue, &evt, portMAX_DELAY) == pdTRUE)
        {
            if (evt.event_id == 2)
            {
                ESP_ERROR_CHECK(i2s_read(I2S_NUM, (void *)&i2s_read_buff[total_bytes / sizeof(uint16_t)], i2s_read_buff_size - total_bytes, &bytes_read, portMAX_DELAY));
                for (int i = 0; i < bytes_read / 2; i++)
                {
                    uint8_t chan = (i2s_read_buff[i] >> 12) & 0x07;
                    uint16_t adc_value = i2s_read_buff[i] & 0xfff;
                    channel_samples_counter[chan]++;
                    adc_samples->samples[sample_counter + i / 8].sample[chan] = calibrate_adc(chan, adc_value);
                }
                total_bytes += bytes_read;
                sample_counter = total_bytes / (CHANNEL_NUM * sizeof(uint16_t));
            }
            else
                ESP_LOGE(TAG, "DMA read error: %d", evt.event_id);
        }
        else
            ESP_LOGE(TAG, "xQueueReceive failed");

        loop_counter++;
    } while (sample_counter < requested_samples);
    int64_t end = esp_timer_get_time() - now;
    ESP_ERROR_CHECK(i2s_adc_disable(I2S_NUM));
    free(i2s_read_buff);
    i2s_read_buff = NULL;

    adc_samples->nr_samples = sample_counter;

    ESP_LOGW(TAG, "i2s_scan: end, time=%lldus, total 8ch samples=%d", end, adc_samples->nr_samples);
    ESP_LOGW(TAG, "%d  %d %d %d %d %d %d %d %d", loop_counter,
             channel_samples_counter[0], channel_samples_counter[1], channel_samples_counter[2], channel_samples_counter[3],
             channel_samples_counter[4], channel_samples_counter[5], channel_samples_counter[6], channel_samples_counter[7]);

    esp_event_post_to(request_scan->receiver.evloop, request_scan->receiver.base, request_scan->receiver.id, (void *)adc_samples, adc_sample_size, (TickType_t)0);
    i2s_free_samples(adc_samples);
    adc_samples = NULL;

    // for (int i = 0; i < sample_counter * 8 && i < 1000; i++)
    //     ESP_LOGI(TAG, "%d %d", i, channel[i]);
}

bool i2sadc_post_sample_request(event_receiver_t *receiver, uint32_t sample_time_us)
{
    request_scan_t ev_data = {*receiver, sample_time_us};
    return (ESP_OK == esp_event_post_to(i2sadc_loop_handle, I2SADC_BASE, I2SADC_SCAN, (void *)&ev_data, sizeof(ev_data), portMAX_DELAY));
}

bool i2sadc_post_set_sample_frequency(event_receiver_t *receiver, uint32_t sample_freq)
{
    request_set_sample_freq_t ev_data = {*receiver, sample_freq};
    return (ESP_OK == esp_event_post_to(i2sadc_loop_handle, I2SADC_BASE, I2SADC_SET_SAMPLE_FREQ, (void *)&ev_data, sizeof(ev_data), portMAX_DELAY));
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
        .task_priority = 7,
        .task_stack_size = 2048 * 3,
        .task_core_id = 1};
    ESP_ERROR_CHECK(esp_event_loop_create(&loop_args, &i2sadc_loop_handle));
    ESP_ERROR_CHECK(esp_event_handler_register_with(i2sadc_loop_handle, I2SADC_BASE, I2SADC_SCAN, &i2s_scan, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register_with(i2sadc_loop_handle, I2SADC_BASE, I2SADC_SET_SAMPLE_FREQ, &i2s_set_sample_freq, NULL));
}

void i2s_init()
{
    i2s_setup_eventloop();
}