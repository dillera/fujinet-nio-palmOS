/* The network stack under the NIO core: NetLib on Palm OS, BSD sockets on a
 * host. Blocking calls with timeouts; the core is single-threaded. */
#ifndef NIO_SOCK_H
#define NIO_SOCK_H

#include <stdint.h>

#define NIO_SOCK_ERROR      (-1)
#define NIO_SOCK_WOULDBLOCK (-2)

/* 0 on success. start may bring a PPP link up and take a while. */
int  nio_sock_start(void);
void nio_sock_stop(void);

/* Resolves host and connects. 0 on success with *sock set. */
int  nio_sock_tcp_connect(const char *host, uint16_t port, long *sock);
/* Bytes sent, or NIO_SOCK_ERROR. */
int  nio_sock_send(long sock, const uint8_t *data, uint16_t len);
/* Bytes read, 0 once the peer has closed, NIO_SOCK_WOULDBLOCK when nothing
 * came within wait_ms, or NIO_SOCK_ERROR. */
int  nio_sock_recv(long sock, uint8_t *data, uint16_t max, uint16_t wait_ms);
void nio_sock_close(long sock);
/* Dotted address of this machine, "" when unknown. */
void nio_sock_local_ip(char *out, uint16_t cap);

#endif
