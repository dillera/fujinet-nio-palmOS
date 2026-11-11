/* nio_sock.h on BSD sockets, for the host tests. */
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "nio_sock.h"

int nio_sock_start(void) { return 0; }
void nio_sock_stop(void) {}

int nio_sock_tcp_connect(const char *host, uint16_t port, long *sock)
{
    struct addrinfo hints, *res, *ai;
    char service[8];
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(service, sizeof(service), "%u", port);
    if (getaddrinfo(host, service, &hints, &res) != 0) return NIO_SOCK_ERROR;
    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) return NIO_SOCK_ERROR;
    *sock = fd;
    return 0;
}

int nio_sock_send(long sock, const uint8_t *data, uint16_t len)
{
    ssize_t n = send((int)sock, data, len, 0);
    return n < 0 ? NIO_SOCK_ERROR : (int)n;
}

int nio_sock_recv(long sock, uint8_t *data, uint16_t max, uint16_t wait_ms)
{
    struct pollfd p;
    ssize_t n;

    p.fd = (int)sock;
    p.events = POLLIN;
    if (poll(&p, 1, wait_ms) == 0) return NIO_SOCK_WOULDBLOCK;
    n = recv((int)sock, data, max, 0);
    return n < 0 ? NIO_SOCK_ERROR : (int)n;
}

void nio_sock_close(long sock) { close((int)sock); }

void nio_sock_local_ip(char *out, uint16_t cap)
{
    snprintf(out, cap, "127.0.0.1");
}
