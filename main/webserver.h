#include "esp_http_server.h"

#ifdef __cplusplus
extern "C"
{
#endif
void webserver_ws_send(httpd_handle_t hd, int socket, char *data);
#ifdef __cplusplus
}
#endif
