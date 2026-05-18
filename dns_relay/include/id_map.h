#ifndef DNS_RELAY_ID_MAP_H
#define DNS_RELAY_ID_MAP_H

#include <stddef.h>
#include <stdint.h>

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

typedef struct id_map id_map;

typedef struct id_map_entry
{
    uint16_t client_id;
    uint16_t upstream_id;
    struct sockaddr_storage client_addr;
    socklen_t client_len;
    uint64_t expire_at_ms;
    char qname[256];
} id_map_entry;

id_map *id_map_create(void);
void id_map_destroy(id_map *m);

/* Allocates upstream_id and stores pending state. Returns 0 on success. */
int id_map_insert(id_map *m, uint16_t client_id,
                  const struct sockaddr_storage *client_addr, socklen_t client_len,
                  const char *qname_optional, uint16_t *upstream_id_out);

/* Finds by upstream_id, removes from map, returns entry (caller must free). */
id_map_entry *id_map_take(id_map *m, uint16_t upstream_id);

size_t id_map_expire(id_map *m, uint64_t now_ms);
uint64_t id_map_next_expire_ms(const id_map *m, uint64_t now_ms);

/* Test / diagnostics: number of pending transactions. */
size_t id_map_pending_count(const id_map *m);

#endif /* DNS_RELAY_ID_MAP_H */
