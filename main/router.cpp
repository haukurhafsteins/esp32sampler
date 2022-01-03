#include <string>
#include <map>
#include <esp_log.h>
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rapidjson/document.h"
#include "router.h"
#include "i2sadc.h"
#include "webserver.h"

using namespace rapidjson;

enum jsonparser_events
{
    EV_WS_DATA,
    EV_I2SADC_DATA,
    EV_I2SADC_FREQ_SET
};

typedef struct
{
    httpd_handle_t hd;
    int socket;
} client_t;

typedef struct
{
    client_t client;
    char payload[64];
} context_ws_t;

static esp_event_loop_handle_t loop_handle;
static esp_event_base_t EV_BASE = "JSONPARSER_BASE";
static std::map<std::string, client_t> parameterMap;

static const char *CMD = "cmd";
static const char *SUBSCRIBE = "subscribe";
static const char *REQUEST = "request";
static const char *PUBLISH = "publish";
static const char *DATA = "data";
static const char *NAME = "name";
static const char *VALUE = "value";
static const char *TAG = "JSON";
static const char *SUBSCRIBE_RESP = "{\"cmd\":\"subscribeResp\", \"data\":{\"name\":\"%s\", \"value\":%s}}";
// static const char *REQUEST_RESP = "{\"cmd\":\"requestResp\", \"data\":{\"name\":\"%s\", \"value\":%s}}";

static uint32_t sample_time_us = 1000;
static uint32_t sample_freq;

static void router_parse_ws_message(client_t client, const char *message)
{
    Document d;
    if (d.Parse(message).HasParseError())
    {
        ESP_LOGE(TAG, "Invalid JSON: %s", message);
        return;
    }

    if (d.HasMember(CMD) && d[CMD].IsString())
    {
        std::string cmdCode = d[CMD].GetString();
        if (cmdCode.compare(SUBSCRIBE) == 0)
        {
            if (d.HasMember(DATA) && d[DATA].IsString())
            {
                char *answer = (char *)calloc(128, sizeof(char));
                std::string par = d[DATA].GetString();
                snprintf(answer, 128, SUBSCRIBE_RESP, par.c_str(), "[0.0]");
                ESP_LOGI(TAG, "Subscription response: %s", answer);
                if (parameterMap.find(par.c_str()) == parameterMap.end())
                    parameterMap[par.c_str()] = client;
                webserver_ws_send(client.hd, client.socket, answer);
            }
        }
        else if (cmdCode.compare(REQUEST) == 0)
        {
            if (d.HasMember(DATA) && d[DATA].IsString())
            {
                std::string par = d[DATA].GetString();
                if (par.compare("ad1Data.0") == 0 || par.compare("ad1Data.1") == 0 || par.compare("ad1Data.2") == 0 || par.compare("ad1Data.3") == 0 ||
                    par.compare("ad1Data.4") == 0 || par.compare("ad1Data.5") == 0 || par.compare("ad1Data.6") == 0 || par.compare("ad1Data.7") == 0 ||
                    par.compare("ad1Data.all") == 0)
                {
                    parameterMap[par.c_str()] = client;
                    event_receiver_t receiver = {loop_handle, EV_BASE, EV_I2SADC_DATA};
                    i2sadc_post_sample_request(&receiver, sample_time_us);
                }
                else
                    ESP_LOGI(TAG, "Unhandled request: %s", par.c_str());
            }
            else
                ESP_LOGI(TAG, "request command doesn't have data element");
        }
        else if (cmdCode.compare(PUBLISH) == 0)
        {
            if (d.HasMember(DATA) && d[DATA].IsObject())
            {
                std::string par = d[DATA][NAME].GetString();
                if (par.compare("sampleTime") == 0)
                {
                    sample_time_us = d[DATA][VALUE].GetInt();
                    ESP_LOGI(TAG, "New sample time: %dus", sample_time_us);
                }
                else if (par.compare("sampleFreq") == 0)
                {
                    sample_freq = d[DATA][VALUE].GetInt();
                    event_receiver_t receiver = {loop_handle, EV_BASE, EV_I2SADC_FREQ_SET};
                    ESP_LOGI(TAG, "New sample frequency: %dHz", sample_freq);
                    i2sadc_post_set_sample_frequency(&receiver, sample_freq);
                }
                else
                    ESP_LOGI(TAG, "Unhandled publish: %s", par.c_str());
            }
            else
                ESP_LOGI(TAG, "publish command doesn't have data element");
        }
        else
            ESP_LOGI(TAG, "Unhandled cmd: %s", message);
    }
}

static void ev_ws_data(void *handler_arg, esp_event_base_t base, int32_t id, void *context)
{
    context_ws_t *context_ws = (context_ws_t *)context;
    ESP_LOGI(TAG, "ev_ws_data: socket %d, payload: %s", context_ws->client.socket, context_ws->payload);
    router_parse_ws_message(context_ws->client, context_ws->payload);
}

static void ev_i2sadc_data(void *handler_arg, esp_event_base_t base, int32_t id, void *context)
{
    i2sadc_samples_t *adc_samples = (i2sadc_samples_t *)context;
    ESP_LOGI(TAG, "ev_i2sadc_data: nr_samples %d", adc_samples->nr_samples);
    char *answer = (char *)calloc(adc_samples->nr_samples * 5 + 64, sizeof(char));

    for (int ch = 0; ch < 8; ch++)
    {
        char buf[32];
        sprintf(buf, "ad1Data.%d", ch);
        if (parameterMap.find(buf) == parameterMap.end())
            continue;

        sprintf(answer, "{\"cmd\":\"requestResp\", \"data\":{\"name\":\"ad1Data.%d\", \"value\":[", ch);
        char b[64];
        int i;
        for (i = 0; i < adc_samples->nr_samples - 1; i++)
        {
            sprintf(b, "%4.2f,", adc_samples->samples[i].sample[ch]);
            strcat(answer, b);
        }
        sprintf(b, "%4.2f]}}", adc_samples->samples[i].sample[ch]);
        strcat(answer, b);

        client_t client = parameterMap[buf];
        webserver_ws_send(client.hd, client.socket, answer);
    }
    free(answer);
}

static void ev_i2sadc_freq_set(void *handler_arg, esp_event_base_t base, int32_t id, void *context)
{
    ESP_LOGI(TAG, "Frequency set to %d", (uint32_t)context);
}

//-----------------------------------------------------------------------
// Public stuff
//-----------------------------------------------------------------------
bool router_post_ws(httpd_handle_t hd, int socket, const char *payload, size_t length)
{
    context_ws_t context_ws;
    context_ws.client.hd = hd;
    context_ws.client.socket = socket;
    snprintf(context_ws.payload, 64, "%s", payload);
    return (ESP_OK == esp_event_post_to(loop_handle, EV_BASE, EV_WS_DATA, (void *)&context_ws, sizeof(context_ws_t), portMAX_DELAY));
}

void router_init()
{
    ESP_EVENT_DECLARE_BASE(EV_BASE);
    esp_event_loop_args_t loop_args = {
        .queue_size = 10,
        .task_name = "router_task",
        .task_priority = 1,
        .task_stack_size = 2048 * 2,
        .task_core_id = 1};

    ESP_ERROR_CHECK(esp_event_loop_create(&loop_args, &loop_handle));
    ESP_ERROR_CHECK(esp_event_handler_register_with(loop_handle, EV_BASE, EV_WS_DATA, &ev_ws_data, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register_with(loop_handle, EV_BASE, EV_I2SADC_DATA, &ev_i2sadc_data, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register_with(loop_handle, EV_BASE, EV_I2SADC_FREQ_SET, &ev_i2sadc_freq_set, NULL));
}