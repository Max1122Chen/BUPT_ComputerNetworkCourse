#include "socket_io.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "dns_relay_constants.h"
#include "debug.h"
#include "net_util.h"

#include <stdio.h>
#include <string.h>

int dns_socket_open(dns_socket *sock, const dns_relay_config *cfg)
{
    int yes = 1;
    struct sockaddr_in up;

    if (sock == NULL || cfg == NULL)
    {
        return -1;
    }

    memset(sock, 0, sizeof(*sock));
    sock->fd = INVALID_SOCKET;

    sock->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock->fd == INVALID_SOCKET)
    {
        return -1;
    }

    if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes)) != 0)
    {
        debug_log(DBG_L1, "socket: SO_REUSEADDR failed (non-fatal)");
    }

    memset(&sock->bind_addr, 0, sizeof(sock->bind_addr));
    sock->bind_addr.sin_family = AF_INET;
    sock->bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    sock->bind_addr.sin_port = htons(DNS_RELAY_PORT);

    if (bind(sock->fd, (struct sockaddr *)&sock->bind_addr, sizeof(sock->bind_addr)) != 0)
    {
        fprintf(stderr, "dnsrelay: bind UDP/%d failed (try Administrator)\n", DNS_RELAY_PORT);
        dns_socket_close(sock);
        return -1;
    }

    memset(&up, 0, sizeof(up));
    up.sin_family = AF_INET;
    up.sin_port = htons(cfg->upstream_port);
    if (inet_pton(AF_INET, cfg->upstream_ip, &up.sin_addr) != 1)
    {
        fprintf(stderr, "dnsrelay: invalid upstream address: %s\n", cfg->upstream_ip);
        dns_socket_close(sock);
        return -1;
    }
    sock->upstream_addr = up;

    return 0;
}

void dns_socket_close(dns_socket *sock)
{
    if (sock == NULL)
    {
        return;
    }
    if (sock->fd != INVALID_SOCKET)
    {
        closesocket(sock->fd);
        sock->fd = INVALID_SOCKET;
    }
}

int dns_socket_recv(dns_socket *sock, uint8_t *buf, size_t cap,
                    struct sockaddr_storage *from, socklen_t *from_len)
{
    int n;

    if (sock == NULL || buf == NULL || from == NULL || from_len == NULL)
    {
        return -1;
    }

    n = recvfrom(sock->fd, (char *)buf, (int)cap, 0, (struct sockaddr *)from, from_len);
    if (n < 0)
    {
        return -1;
    }
    return n;
}

int dns_socket_sendto(dns_socket *sock, const uint8_t *buf, size_t len,
                      const struct sockaddr_storage *to, socklen_t to_len)
{
    int n;

    if (sock == NULL || buf == NULL || to == NULL)
    {
        return -1;
    }

    n = sendto(sock->fd, (const char *)buf, (int)len, 0, (const struct sockaddr *)to, to_len);
    if (n < 0)
    {
        return -1;
    }
    return n;
}

int dns_socket_is_upstream_source(const dns_socket *sock,
                                  const struct sockaddr_storage *from,
                                  socklen_t from_len)
{
    struct sockaddr_storage up_st;
    socklen_t up_len = (socklen_t)sizeof(up_st);

    if (sock == NULL || from == NULL)
    {
        return 0;
    }

    memset(&up_st, 0, sizeof(up_st));
    memcpy(&up_st, &sock->upstream_addr, sizeof(sock->upstream_addr));
    up_st.ss_family = AF_INET;
    up_len = (socklen_t)sizeof(struct sockaddr_in);

    return net_util_addr_equals(from, from_len, &up_st, up_len);
}
