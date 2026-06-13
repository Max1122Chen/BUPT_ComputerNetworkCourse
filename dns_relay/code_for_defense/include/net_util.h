#ifndef DNS_RELAY_NET_UTIL_H
#define DNS_RELAY_NET_UTIL_H

#include <stddef.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

int net_util_addr_equals(const struct sockaddr_storage *a, socklen_t a_len,
                         const struct sockaddr_storage *b, socklen_t b_len);

char *net_util_format_endpoint(const struct sockaddr_storage *addr, socklen_t len,
                               char *buf, size_t buf_len);

#endif
