#include "id_map.h"

#include "dns_relay_constants.h"
#include "debug.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>

typedef struct id_map_node
{
    id_map_entry data;
    struct id_map_node *next;
} id_map_node;

struct id_map
{
    id_map_node *buckets[DNS_ID_MAP_BUCKET_COUNT];
    size_t count;
    uint16_t next_id;
};

static unsigned bucket_index(uint16_t upstream_id)
{
    return (unsigned)(upstream_id % DNS_ID_MAP_BUCKET_COUNT);
}

static int id_in_use(const id_map *m, uint16_t id)
{
    const id_map_node *n;
    unsigned b = bucket_index(id);

    for (n = m->buckets[b]; n != NULL; n = n->next)
    {
        if (n->data.upstream_id == id)
        {
            return 1;
        }
    }
    return 0;
}

id_map *id_map_create(void)
{
    id_map *m = (id_map *)calloc(1, sizeof(*m));
    if (m != NULL)
    {
        m->next_id = 1;
    }
    return m;
}

void id_map_destroy(id_map *m)
{
    size_t i;

    if (m == NULL)
    {
        return;
    }

    for (i = 0; i < DNS_ID_MAP_BUCKET_COUNT; ++i)
    {
        id_map_node *n = m->buckets[i];
        while (n != NULL)
        {
            id_map_node *next = n->next;
            free(n);
            n = next;
        }
    }
    free(m);
}

static uint16_t alloc_upstream_id(id_map *m)
{
    uint16_t tries;

    for (tries = 0; tries < 65535; ++tries)
    {
        uint16_t id = m->next_id;
        m->next_id++;
        if (m->next_id == 0)
        {
            m->next_id = 1;
        }
        if (!id_in_use(m, id))
        {
            return id;
        }
    }
    return 0;
}

int id_map_insert(id_map *m, uint16_t client_id,
                  const struct sockaddr_storage *client_addr, socklen_t client_len,
                  const char *qname_optional, uint16_t *upstream_id_out)
{
    id_map_node *node;
    uint16_t uid;
    unsigned b;

    if (m == NULL || client_addr == NULL || upstream_id_out == NULL)
    {
        return -1;
    }
    if (m->count >= DNS_ID_MAP_MAX_PENDING)
    {
        return -1;
    }

    uid = alloc_upstream_id(m);
    if (uid == 0)
    {
        return -1;
    }

    node = (id_map_node *)calloc(1, sizeof(*node));
    if (node == NULL)
    {
        return -1;
    }

    node->data.client_id = client_id;
    node->data.upstream_id = uid;
    memcpy(&node->data.client_addr, client_addr, sizeof(node->data.client_addr));
    node->data.client_len = client_len;
    node->data.expire_at_ms = platform_monotonic_ms() + DNS_ID_MAP_TIMEOUT_MS;
    if (qname_optional != NULL)
    {
        strncpy(node->data.qname, qname_optional, sizeof(node->data.qname) - 1);
    }

    b = bucket_index(uid);
    node->next = m->buckets[b];
    m->buckets[b] = node;
    m->count++;

    *upstream_id_out = uid;
    return 0;
}

id_map_entry *id_map_take(id_map *m, uint16_t upstream_id)
{
    unsigned b = bucket_index(upstream_id);
    id_map_node **pp = &m->buckets[b];
    id_map_entry *out;

    while (*pp != NULL)
    {
        if ((*pp)->data.upstream_id == upstream_id)
        {
            id_map_node *node = *pp;
            *pp = node->next;
            m->count--;

            out = (id_map_entry *)malloc(sizeof(*out));
            if (out == NULL)
            {
                free(node);
                return NULL;
            }
            *out = node->data;
            free(node);
            return out;
        }
        pp = &(*pp)->next;
    }
    return NULL;
}

size_t id_map_expire(id_map *m, uint64_t now_ms)
{
    size_t removed = 0;
    size_t i;

    if (m == NULL)
    {
        return 0;
    }

    for (i = 0; i < DNS_ID_MAP_BUCKET_COUNT; ++i)
    {
        id_map_node **pp = &m->buckets[i];
        while (*pp != NULL)
        {
            if ((*pp)->data.expire_at_ms <= now_ms)
            {
                id_map_node *dead = *pp;
                *pp = dead->next;
                debug_log(DBG_L1, "id_map: timeout upstream_id=%u qname=%s",
                          (unsigned)dead->data.upstream_id, dead->data.qname);
                free(dead);
                m->count--;
                removed++;
            }
            else
            {
                pp = &(*pp)->next;
            }
        }
    }
    return removed;
}

uint64_t id_map_next_expire_ms(const id_map *m, uint64_t now_ms)
{
    uint64_t best = RELAY_SELECT_MAX_MS;
    size_t i;

    if (m == NULL || m->count == 0)
    {
        return RELAY_SELECT_MAX_MS;
    }

    for (i = 0; i < DNS_ID_MAP_BUCKET_COUNT; ++i)
    {
        const id_map_node *n;
        for (n = m->buckets[i]; n != NULL; n = n->next)
        {
            uint64_t remain;
            if (n->data.expire_at_ms <= now_ms)
            {
                return 0;
            }
            remain = n->data.expire_at_ms - now_ms;
            if (remain < best)
            {
                best = remain;
            }
        }
    }
    return best;
}

size_t id_map_pending_count(const id_map *m)
{
    return m != NULL ? m->count : 0;
}
