
#include <stdio.h>
#include <time.h>
#include "esp_log.h"
#include "esp_sntp.h"

static int64_t msUnixStarttime;
static int64_t usUnixStarttime;

static const char *TAG = "Time Manager";

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

void time_init()
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
    setenv("TZ", "GMT-0", 1);
    tzset();

    // wait for time to be set
    char strftime_buf[64];
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);

    time_t msUnixTime = time(NULL);
    int64_t usSinceStart = esp_timer_get_time();
    msUnixStarttime = msUnixTime + usSinceStart/1000;
    usUnixStarttime = msUnixTime*1000 + usSinceStart;

    ESP_LOGI(TAG, "Milliseconds Unix time      : %ld", msUnixTime);
    ESP_LOGI(TAG, "Microseconds since start    : %lld", usSinceStart);
    ESP_LOGI(TAG, "Milliseconds Unis start-time: %lld", msUnixStarttime);
    ESP_LOGI(TAG, "Microseconds Unix start-time: %llu", usUnixStarttime);
}

int64_t timestamp_ms()
{
    return msUnixStarttime + esp_timer_get_time()/1000;
}

int64_t timestamp_us()
{
    return usUnixStarttime + esp_timer_get_time();
}