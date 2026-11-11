/* Drives the NIO core through fujinet-nio-lib's public calls, as a Palm app
 * does, against tests/servers.py on this machine.
 *   host_test <http-port> <echo-port> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fujinet-nio.h"

static int failures;

#define CHECK(what, ok) do { \
    if (ok) printf("ok   %s\n", what); \
    else { printf("FAIL %s\n", what); failures++; } } while (0)

/* Reads until end of stream, waiting out NotReady. */
static uint8_t read_all(fn_handle_t h, char *buf, uint16_t cap, uint16_t *total)
{
    uint16_t got;
    uint8_t flags = 0, err;
    int idle = 0;

    *total = 0;
    while (*total < cap && idle < 50) {
        err = fn_read(h, *total, (uint8_t *)buf + *total, (uint16_t)(cap - *total), &got, &flags);
        if (err == FN_ERR_NOT_READY) {
            usleep(20000);
            idle++;
            continue;
        }
        if (err != FN_OK) return err;
        *total += got;
        if (flags & FN_READ_EOF) return FN_OK;
    }
    return FN_ERR_TIMEOUT;
}

int main(int argc, char **argv)
{
    char url[128], buf[4096], big[3000];
    fn_fuji_info_t info;
    fn_wifi_status_t wifi;
    fn_wifi_config_t config;
    fn_handle_t h;
    uint16_t status, total, got, written;
    uint32_t length;
    uint8_t flags, err;
    int http_port, echo_port, i;

    if (argc < 3) return 2;
    http_port = atoi(argv[1]);
    echo_port = atoi(argv[2]);

    CHECK("fn_init", fn_init() == FN_OK);
    err = fn_fuji_get_info(&info);
    CHECK("fuji info", err == FN_OK && strcmp(info.profile, "palmos-netlib") == 0);
    err = fn_wifi_get_status(&wifi);
    CHECK("wifi status", err == FN_OK && wifi.link_state == 2 && wifi.ip[0]);
    err = fn_wifi_get_config(&config);
    CHECK("wifi config", err == FN_OK && config.ssid[0]);

    /* GET, body larger than one packet. */
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/big", http_port);
    err = fn_open(&h, FN_METHOD_GET, url, 0);
    CHECK("GET open", err == FN_OK);
    do err = fn_info(h, &status, &length, &flags); while (err == FN_ERR_NOT_READY);
    CHECK("GET info", err == FN_OK && status == 200 && length == 3000);
    err = read_all(h, buf, sizeof(buf), &total);
    for (i = 0; i < 3000; i++) big[i] = (char)('a' + i % 26);
    CHECK("GET body", err == FN_OK && total == 3000 && memcmp(buf, big, 3000) == 0);
    CHECK("GET close", fn_close(h) == FN_OK);

    /* 404, and a server that sends no Content-Length. */
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/missing", http_port);
    err = fn_open(&h, FN_METHOD_GET, url, 0);
    do err = fn_info(h, &status, &length, &flags); while (err == FN_ERR_NOT_READY);
    CHECK("404 status", err == FN_OK && status == 404);
    fn_close(h);
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/nolength", http_port);
    err = fn_open(&h, FN_METHOD_GET, url, 0);
    err = read_all(h, buf, sizeof(buf), &total);
    CHECK("body without length", err == FN_OK && total == 11 && memcmp(buf, "until close", 11) == 0);
    fn_close(h);

    /* POST: the server echoes the body back. */
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/echo", http_port);
    err = fn_open(&h, FN_METHOD_POST, url, FN_OPEN_BODY_UNKNOWN);
    if (err == FN_OK) err = fn_write(h, 0, (const uint8_t *)"hello palm", 10, &written);
    if (err == FN_OK) err = fn_write(h, 10, 0, 0, &written);
    CHECK("POST write", err == FN_OK);
    err = read_all(h, buf, sizeof(buf), &total);
    CHECK("POST echo", err == FN_OK && total == 10 && memcmp(buf, "hello palm", 10) == 0);
    fn_close(h);

    /* https is refused, not mangled. */
    err = fn_open(&h, FN_METHOD_GET, "https://example.com/", 0);
    CHECK("https unsupported", err == FN_ERR_UNSUPPORTED);

    /* TCP: echo server, then the peer closing. */
    err = fn_tcp_open(&h, "127.0.0.1", (uint16_t)echo_port);
    CHECK("tcp open", err == FN_OK);
    err = fn_read(h, 0, (uint8_t *)buf, 16, &got, &flags);
    CHECK("tcp read before data is NotReady", err == FN_ERR_NOT_READY);
    err = fn_write(h, 0, (const uint8_t *)"ping\n", 5, &written);
    CHECK("tcp write", err == FN_OK && written == 5);
    total = 0;
    for (i = 0; i < 100 && total < 5; i++) {
        err = fn_read(h, total, (uint8_t *)buf + total, 16, &got, &flags);
        if (err == FN_OK) total += got;
        else usleep(10000);
    }
    CHECK("tcp echo", total == 5 && memcmp(buf, "ping\n", 5) == 0);
    err = fn_write(h, 5, (const uint8_t *)"quit\n", 5, &written);
    flags = 0;
    for (i = 0; i < 100 && !(flags & FN_READ_EOF); i++) {
        err = fn_read(h, total, (uint8_t *)buf, 16, &got, &flags);
        if (err == FN_OK) total += got;
        else usleep(10000);
    }
    CHECK("tcp peer close gives EOF", (flags & FN_READ_EOF) != 0);
    CHECK("tcp close", fn_close(h) == FN_OK);
    err = fn_tcp_open(&h, "127.0.0.1", 1);
    CHECK("tcp refused is an error", err != FN_OK);

    fn_shutdown();
    printf("%s\n", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
