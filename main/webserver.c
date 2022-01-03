#include <esp_log.h>
#include "esp_http_server.h"
#include "esp_event.h"
#include "router.h"
#include "i2sadc.h"
#include "time_manager.h"

#define MIN(a, b) ((a) > (b) ? (b) : (a))

static const char *TAG = "webserver";

#define ADCBUFSIZE 4096

extern time_t time_ms();

struct async_resp_arg
{
    httpd_handle_t hd;
    int fd;
};

static httpd_handle_t server = NULL;

esp_err_t get_handler(httpd_req_t *req)
{
    char buf[128];
    // httpd_resp_send_chunk(req, "<html><head</head><body><pre style=\"word-wrap: break-word; white-space: pre-wrap;\">", HTTPD_RESP_USE_STRLEN);
    snprintf(buf, 128, "# HELP adc_%d AD value for ADC converter %d\n", 0, 0);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    snprintf(buf, 128, "# TYPE adc_%d gauge\n", 0);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);

    /*
    i2sadc_channel_t* channel = &channels.channel[0];
    int64_t timestamp = timestamp_ms();
    int i;
    for (i=0;i<10;i++) {
        snprintf(buf, 128, "adc_%d %d %lld\n", 0, channel->data[i], timestamp+i);
        httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    }*/
    // httpd_resp_send_chunk(req, "</pre></body></html>", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, buf, 0);

    ESP_LOGI(TAG, "GET finished");
    return ESP_OK;
}

esp_err_t post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST");

    /* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */
    char content[100];

    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0)
    {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            httpd_resp_send_408(req);
        return ESP_FAIL;
    }

    const char resp[] = "URI POST Response";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    uint8_t *buf = (uint8_t *)calloc(256, sizeof(uint8_t));
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = buf;
    ESP_ERROR_CHECK(httpd_ws_recv_frame(req, &ws_pkt, 256));
    //ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    router_post_ws(req->handle, httpd_req_to_sockfd(req), (const char *)ws_pkt.payload, ws_pkt.len + 1);
    free(buf);
    return ESP_OK;
}

httpd_uri_t uri_get = {
    .uri = "/metrics",
    .method = HTTP_GET,
    .handler = get_handler,
    .user_ctx = NULL};

httpd_uri_t uri_post = {
    .uri = "/metrics",
    .method = HTTP_POST,
    .handler = post_handler,
    .user_ctx = NULL};

httpd_uri_t ws = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = ws_handler,
    .user_ctx = NULL,
    .is_websocket = true};

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_post);
        httpd_register_uri_handler(server, &ws);
    }
    return server;
}

void webserver_ws_send(httpd_handle_t hd, int socket, char *data)
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    while (ESP_OK != httpd_ws_send_frame_async(hd, socket, &ws_pkt))
        vTaskDelay(pdMS_TO_TICKS(10));
}

void webserver_init()
{
    start_webserver();
}
