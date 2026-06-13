#ifndef DNS_RELAY_SOCKET_IO_H
#define DNS_RELAY_SOCKET_IO_H

#include <stddef.h>
#include <stdint.h>

#include "config.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#endif

typedef struct dns_socket
{
    SOCKET fd;
    struct sockaddr_in bind_addr;
    struct sockaddr_in upstream_addr;
} dns_socket;

int dns_socket_open(dns_socket *sock, const dns_relay_config *cfg);
void dns_socket_close(dns_socket *sock);

int dns_socket_recv(dns_socket *sock, uint8_t *buf, size_t cap,
                    struct sockaddr_storage *from, socklen_t *from_len);

int dns_socket_sendto(dns_socket *sock, const uint8_t *buf, size_t len,
                      const struct sockaddr_storage *to, socklen_t to_len);

int dns_socket_is_upstream_source(const dns_socket *sock,
                                  const struct sockaddr_storage *from,
                                  socklen_t from_len);

#endif
