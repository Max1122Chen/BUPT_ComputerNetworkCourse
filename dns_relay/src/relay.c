#include "relay.h"

#include "dns_packet.h"
#include "dns_relay_constants.h"
#include "debug.h"
#include "id_map.h"
#include "net_util.h"
#include "platform.h"
#include "socket_io.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile int s_stop_requested;

void relay_request_stop(void)
{
    s_stop_requested = 1;
}

static int relay_forward(dns_socket *sock, id_map *map, const uint8_t *buf, size_t len,
                         uint16_t client_id, const struct sockaddr_storage *client_addr,
                         socklen_t client_len, const char *qname, unsigned *seq)
{
    uint8_t out[DNS_UDP_BUF_SIZE];
    uint16_t upstream_id;
    struct sockaddr_storage up_st;
    socklen_t up_len;
    char ep[64];

    if (len > DNS_UDP_BUF_SIZE)
    {
        return -1;
    }

    if (id_map_insert(map, client_id, client_addr, client_len, qname, &upstream_id) != 0)
    {
        debug_log(DBG_L1, "relay: id_map full, drop query");
        return -1;
    }

    memcpy(out, buf, len);
    dns_packet_set_id(out, upstream_id);

    memset(&up_st, 0, sizeof(up_st));
    memcpy(&up_st, &sock->upstream_addr, sizeof(sock->upstream_addr));
    up_st.ss_family = AF_INET;
    up_len = (socklen_t)sizeof(struct sockaddr_in);

    if (dns_socket_sendto(sock, out, len, &up_st, up_len) < 0)
    {
        (void)id_map_take(map, upstream_id);
        return -1;
    }

  if (debug_get_level() >= DBG_L1)
    {
        (*seq)++;
        net_util_format_endpoint(client_addr, client_len, ep, sizeof(ep));
        debug_log(DBG_L1, "#%u relay -> upstream id %u->%u client %s qname=%s", *seq,
                  (unsigned)client_id, (unsigned)upstream_id, ep,
                  qname != NULL ? qname : "?");
    }

    if (debug_get_level() >= DBG_L2)
    {
        debug_log(DBG_L2, "relay: forwarded %u bytes upstream_id=%u", (unsigned)len,
                  (unsigned)upstream_id);
    }

    return 0;
}

static void handle_client_query(dns_socket *sock, id_map *map, const uint8_t *buf, size_t len,
                                const struct sockaddr_storage *from, socklen_t from_len,
                                unsigned *seq)
{
    dns_query_info qinfo;
    uint16_t client_id;

    if (len > DNS_UDP_BUF_SIZE)
    {
        debug_log(DBG_L2, "relay: drop oversize client packet (%u)", (unsigned)len);
        return;
    }

    if (dns_packet_parse_query(buf, len, &qinfo) != 0)
    {
        debug_log(DBG_L2, "relay: drop unparseable query (%u bytes)", (unsigned)len);
        return;
    }

    client_id = qinfo.id;
    (void)relay_forward(sock, map, buf, len, client_id, from, from_len, qinfo.qname, seq);
}

static void handle_upstream_response(dns_socket *sock, id_map *map, uint8_t *buf, size_t len,
                                     unsigned *seq)
{
    uint16_t upstream_id;
    id_map_entry *entry;
    char ep[64];

    if (len > DNS_UDP_BUF_SIZE)
    {
        debug_log(DBG_L2, "relay: drop oversize upstream packet (%u)", (unsigned)len);
        return;
    }

    upstream_id = dns_packet_get_id(buf);
    entry = id_map_take(map, upstream_id);
    if (entry == NULL)
    {
        debug_log(DBG_L2, "relay: unknown or late upstream id %u", (unsigned)upstream_id);
        return;
    }

    dns_packet_set_id(buf, entry->client_id);

    if (dns_socket_sendto(sock, buf, len, &entry->client_addr, entry->client_len) < 0)
    {
        debug_log(DBG_L1, "relay: sendto client failed");
        free(entry);
        return;
    }

    if (debug_get_level() >= DBG_L1)
    {
        (*seq)++;
        net_util_format_endpoint(&entry->client_addr, entry->client_len, ep, sizeof(ep));
        debug_log(DBG_L1, "#%u relay <- upstream id %u->%u client %s", *seq,
                  (unsigned)upstream_id, (unsigned)entry->client_id, ep);
    }

    free(entry);
}

int relay_run(const dns_relay_config *cfg)
{
    dns_socket sock;
    id_map *map = NULL;
    uint8_t buf[DNS_UDP_BUF_SIZE];
    struct sockaddr_storage from;
    socklen_t from_len;
    fd_set readfds;
    unsigned seq = 0;

    if (cfg == NULL)
    {
        return -1;
    }

    s_stop_requested = 0;

    if (dns_socket_open(&sock, cfg) != 0)
    {
        return -1;
    }

    map = id_map_create();
    if (map == NULL)
    {
        dns_socket_close(&sock);
        return -1;
    }

    fprintf(stderr, "dnsrelay: listening UDP/%d, upstream %s:%u (Release 1 relay)\n",
            DNS_RELAY_PORT, cfg->upstream_ip, (unsigned)cfg->upstream_port);
    debug_log(DBG_L1, "relay: started debug_level=%d", cfg->debug_level);

    while (!s_stop_requested)
    {
        uint64_t now = platform_monotonic_ms();
        uint64_t wait_ms = id_map_next_expire_ms(map, now);
        struct timeval tv;

        if (wait_ms > RELAY_SELECT_MAX_MS)
        {
            wait_ms = RELAY_SELECT_MAX_MS;
        }

        FD_ZERO(&readfds);
        FD_SET(sock.fd, &readfds);

        tv.tv_sec = (long)(wait_ms / 1000);
        tv.tv_usec = (long)((wait_ms % 1000) * 1000);

        if (select((int)sock.fd + 1, &readfds, NULL, NULL, &tv) < 0)
        {
            break;
        }

        (void)id_map_expire(map, platform_monotonic_ms());

        if (FD_ISSET(sock.fd, &readfds))
        {
            from_len = (socklen_t)sizeof(from);
            int n = dns_socket_recv(&sock, buf, sizeof(buf), &from, &from_len);
            if (n > 0)
            {
                if (dns_socket_is_upstream_source(&sock, &from, from_len))
                {
                    handle_upstream_response(&sock, map, buf, (size_t)n, &seq);
                }
                else
                {
                    handle_client_query(&sock, map, buf, (size_t)n, &from, from_len, &seq);
                }
            }
        }
    }

    id_map_destroy(map);
    dns_socket_close(&sock);
    fprintf(stderr, "dnsrelay: stopped\n");
    return 0;
}
