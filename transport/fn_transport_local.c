/* fujinet-nio-lib transport that reaches no wire: each request packet goes
 * straight to the NIO core in this process. Link it in place of the lib's
 * serial or Springboard transport and an app's fn_* calls run unchanged. */
#include "fujinet-nio.h"
#include "fn_internal.h"
#include "fn_platform.h"
#include "nio.h"

static NioState gNio;
static uint8_t gOpen;

uint8_t fn_transport_init(void)
{
    if (!gOpen) {
        nio_init(&gNio);
        gOpen = 1;
    }
    return FN_OK;
}

void fn_transport_close(void)
{
    if (gOpen) nio_shutdown(&gNio);
    gOpen = 0;
}

uint8_t fn_transport_ready(void)
{
    return gOpen;
}

uint8_t fn_transport_exchange_buffers(const uint8_t *request, uint16_t request_length,
                                      uint8_t *response, uint16_t response_capacity,
                                      uint16_t *response_length)
{
    if (!response_length) return FN_ERR_INVALID;
    *response_length = 0;
    if (!gOpen) return FN_ERR_NOT_FOUND;
    if (!request || !response) return FN_ERR_INVALID;
    *response_length = nio_exchange(&gNio, request, request_length, response, response_capacity);
    return *response_length ? FN_OK : FN_ERR_INVALID;
}

uint8_t fn_transport_exchange(void)
{
    return fn_transport_exchange_buffers(_fn_transport_ctx.request, _fn_transport_ctx.req_len,
                                         _fn_transport_ctx.response, _fn_transport_ctx.resp_max,
                                         &_fn_transport_ctx.resp_len);
}

const char *fn_platform_name(void)
{
    return "palmos-local";
}
