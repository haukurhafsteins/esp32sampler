#include "esp_http_server.h"

#ifdef __cplusplus
extern "C"
{
#endif
void router_init();
bool router_post_ws(httpd_handle_t hd, int socket, const char *data, size_t length);

#ifdef __cplusplus
} // extern "C"
#endif