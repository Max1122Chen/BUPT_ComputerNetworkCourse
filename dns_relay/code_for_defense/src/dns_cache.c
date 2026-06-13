#include "dns_cache.h"

#include "dns_relay_constants.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>

typedef struct dns_cache_node
{
    char *qname;
    uint32_t ipv4_be;
    uint64_t expire_at_ms;
    struct dns_cache_node *hash_next;
    struct dns_cache_node *lru_prev;
    struct dns_cache_node *lru_next;
} dns_cache_node;

struct dns_cache
{
    dns_cache_node **buckets;
    size_t bucket_count;
    size_t capacity;
    size_t size;
    dns_cache_node lru_sentinel;
};

static unsigned dns_cache_hash(const char *s)
{
    uint32_t h = 2166136261u;
    const unsigned char *p = (const unsigned char *)s;

    while (*p != 0)
    {
        h ^= (uint32_t)(*p++);
        h *= 16777619u;
    }
    return (unsigned)h;
}

static void qname_to_lower_inplace(char *s)
{
    size_t i;

    if (s == NULL)
    {
        return;
    }

    for (i = 0; s[i] != '\0'; ++i)
    {
        char c = s[i];
        if (c >= 'A' && c <= 'Z')
        {
            s[i] = (char)(c - 'A' + 'a');
        }
    }

    if (i > 0 && s[i - 1] == '.')
    {
        s[i - 1] = '\0';
    }
}

static void lru_push_front(dns_cache *c, dns_cache_node *node)
{
    node->lru_prev = &c->lru_sentinel;
    node->lru_next = c->lru_sentinel.lru_next;
    c->lru_sentinel.lru_next->lru_prev = node;
    c->lru_sentinel.lru_next = node;
}

static void lru_remove(dns_cache_node *node)
{
    node->lru_prev->lru_next = node->lru_next;
    node->lru_next->lru_prev = node->lru_prev;
}

static dns_cache_node *lru_pop_back(dns_cache *c)
{
    dns_cache_node *tail = c->lru_sentinel.lru_prev;

    if (tail == &c->lru_sentinel)
    {
        return NULL;
    }

    lru_remove(tail);
    return tail;
}

static void cache_remove_node(dns_cache *c, dns_cache_node *node)
{
    unsigned b;
    dns_cache_node **pp;

    if (c == NULL || node == NULL || node->qname == NULL)
    {
        return;
    }

    b = dns_cache_hash(node->qname) % (unsigned)c->bucket_count;
    pp = &c->buckets[b];
    while (*pp != NULL)
    {
        if (*pp == node)
        {
            *pp = node->hash_next;
            break;
        }
        pp = &(*pp)->hash_next;
    }

    lru_remove(node);
    free(node->qname);
    free(node);
    if (c->size > 0)
    {
        c->size--;
    }
}

static dns_cache_node *cache_find_node(dns_cache *c, const char *qname_lower)
{
    unsigned b;
    dns_cache_node *node;

    b = dns_cache_hash(qname_lower) % (unsigned)c->bucket_count;
    for (node = c->buckets[b]; node != NULL; node = node->hash_next)
    {
        if (strcmp(node->qname, qname_lower) == 0)
        {
            return node;
        }
    }
    return NULL;
}

dns_cache *dns_cache_create(size_t capacity)
{
    dns_cache *c = (dns_cache *)calloc(1, sizeof(*c));

    if (c == NULL)
    {
        return NULL;
    }

    if (capacity == 0)
    {
        capacity = DNS_CACHE_DEFAULT_CAPACITY;
    }

    c->capacity = capacity;
    c->bucket_count = capacity * 2;
    if (c->bucket_count < 64)
    {
        c->bucket_count = 64;
    }

    c->buckets = (dns_cache_node **)calloc(c->bucket_count, sizeof(c->buckets[0]));
    if (c->buckets == NULL)
    {
        free(c);
        return NULL;
    }

    c->lru_sentinel.lru_next = &c->lru_sentinel;
    c->lru_sentinel.lru_prev = &c->lru_sentinel;
    return c;
}

void dns_cache_destroy(dns_cache *c)
{
    if (c == NULL)
    {
        return;
    }

    while (c->lru_sentinel.lru_next != &c->lru_sentinel)
    {
        cache_remove_node(c, c->lru_sentinel.lru_next);
    }

    free(c->buckets);
    free(c);
}

dns_cache_lookup_result dns_cache_lookup(dns_cache *c, const char *qname,
                                         uint32_t *ipv4_out, uint64_t now_ms)
{
    char tmp[DNS_QNAME_MAX_LEN];
    dns_cache_node *node;

    if (c == NULL || qname == NULL)
    {
        return DNS_CACHE_MISS;
    }

    if (strlen(qname) >= sizeof(tmp))
    {
        return DNS_CACHE_MISS;
    }

    strcpy(tmp, qname);
    qname_to_lower_inplace(tmp);

    node = cache_find_node(c, tmp);
    if (node == NULL)
    {
        return DNS_CACHE_MISS;
    }

    if (node->expire_at_ms <= now_ms)
    {
        cache_remove_node(c, node);
        return DNS_CACHE_EXPIRED;
    }

    lru_remove(node);
    lru_push_front(c, node);

    if (ipv4_out != NULL)
    {
        *ipv4_out = node->ipv4_be;
    }
    return DNS_CACHE_HIT;
}

int dns_cache_insert(dns_cache *c, const char *qname, uint32_t ipv4_be,
                     uint32_t ttl_sec, uint64_t now_ms)
{
    char tmp[DNS_QNAME_MAX_LEN];
    unsigned b;
    dns_cache_node *node;
    uint64_t expire_at;

    if (c == NULL || qname == NULL || ttl_sec == 0)
    {
        return -1;
    }

    if (strlen(qname) >= sizeof(tmp))
    {
        return -1;
    }

    strcpy(tmp, qname);
    qname_to_lower_inplace(tmp);

    expire_at = now_ms + (uint64_t)ttl_sec * 1000u;

    node = cache_find_node(c, tmp);
    if (node != NULL)
    {
        node->ipv4_be = ipv4_be;
        node->expire_at_ms = expire_at;
        lru_remove(node);
        lru_push_front(c, node);
        return 0;
    }

    while (c->size >= c->capacity)
    {
        dns_cache_node *evicted = lru_pop_back(c);
        if (evicted == NULL)
        {
            break;
        }
        cache_remove_node(c, evicted);
    }

    node = (dns_cache_node *)calloc(1, sizeof(*node));
    if (node == NULL)
    {
        return -1;
    }

    node->qname = (char *)calloc(1, strlen(tmp) + 1);
    if (node->qname == NULL)
    {
        free(node);
        return -1;
    }
    strcpy(node->qname, tmp);
    node->ipv4_be = ipv4_be;
    node->expire_at_ms = expire_at;

    b = dns_cache_hash(tmp) % (unsigned)c->bucket_count;
    node->hash_next = c->buckets[b];
    c->buckets[b] = node;
    lru_push_front(c, node);
    c->size++;

    debug_log(DBG_L2, "dns_cache: insert qname=%s ttl=%u size=%u",
              tmp, (unsigned)ttl_sec, (unsigned)c->size);
    return 0;
}

size_t dns_cache_size(const dns_cache *c)
{
    if (c == NULL)
    {
        return 0;
    }
    return c->size;
}
