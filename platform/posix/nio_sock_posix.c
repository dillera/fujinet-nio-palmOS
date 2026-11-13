/* nio_sock.h on BSD sockets, for the host tests. */
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "nio_sock.h"

int nio_sock_start(NioNet *net) { (void)net; return 0; }
void nio_sock_stop(NioNet *net) { (void)net; }

int nio_sock_tcp_connect(NioNet *net, const char *host, uint16_t port, long *sock)
{
    (void)net;
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

int nio_sock_send(NioNet *net, long sock, const uint8_t *data, uint16_t len)
{
    (void)net;
    ssize_t n = send((int)sock, data, len, 0);
    return n < 0 ? NIO_SOCK_ERROR : (int)n;
}

int nio_sock_recv(NioNet *net, long sock, uint8_t *data, uint16_t max, uint16_t wait_ms)
{
    (void)net;
    struct pollfd p;
    ssize_t n;

    p.fd = (int)sock;
    p.events = POLLIN;
    if (poll(&p, 1, wait_ms) == 0) return NIO_SOCK_WOULDBLOCK;
    n = recv((int)sock, data, max, 0);
    return n < 0 ? NIO_SOCK_ERROR : (int)n;
}

void nio_sock_close(NioNet *net, long sock) { (void)net; close((int)sock); }

void nio_sock_local_ip(NioNet *net, char *out, uint16_t cap)
{
    (void)net;
    snprintf(out, cap, "127.0.0.1");
}
