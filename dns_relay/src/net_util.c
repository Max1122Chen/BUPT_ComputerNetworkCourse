#include "net_util.h"

#include <stdio.h>
#include <string.h>

int net_util_addr_equals(const struct sockaddr_storage *a, socklen_t a_len,
                         const struct sockaddr_storage *b, socklen_t b_len)
{
    const struct sockaddr_in *ina;
    const struct sockaddr_in *inb;

    if (a == NULL || b == NULL || a_len < (socklen_t)sizeof(struct sockaddr_in) ||
        b_len < (socklen_t)sizeof(struct sockaddr_in))
    {
        return 0;
    }

    if (a->ss_family != AF_INET || b->ss_family != AF_INET)
    {
        return 0;
    }

    ina = (const struct sockaddr_in *)a;
    inb = (const struct sockaddr_in *)b;

    return ina->sin_addr.s_addr == inb->sin_addr.s_addr && ina->sin_port == inb->sin_port;
}

char *net_util_format_endpoint(const struct sockaddr_storage *addr, socklen_t len,
                               char *buf, size_t buf_len)
{
    const struct sockaddr_in *in;
    char ip[INET_ADDRSTRLEN];

    if (buf == NULL || buf_len == 0)
    {
        return buf;
    }

    buf[0] = '\0';
    if (addr == NULL || len < (socklen_t)sizeof(struct sockaddr_in) || addr->ss_family != AF_INET)
    {
        return buf;
    }

    in = (const struct sockaddr_in *)addr;
    if (inet_ntop(AF_INET, &in->sin_addr, ip, sizeof(ip)) == NULL)
    {
        snprintf(buf, buf_len, "?");
        return buf;
    }

    snprintf(buf, buf_len, "%s:%u", ip, (unsigned)ntohs(in->sin_port));
    return buf;
}
