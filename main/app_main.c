#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "time_manager.h"
#include "router.h"

extern void webserver_init(void);
extern void wifi_init(void);
extern void i2s_init();

static const char *TAG = "Main";

static void on_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    time_init();
    webserver_init();
    i2s_init();

    ESP_LOGI(TAG, "Initialization finished");
}

esp_err_t app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    esp_timer_init();
    router_init();
    wifi_init();

    return ESP_OK;
}
