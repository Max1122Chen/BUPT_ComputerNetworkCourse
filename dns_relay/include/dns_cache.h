#ifndef DNS_RELAY_DNS_CACHE_H
#define DNS_RELAY_DNS_CACHE_H

#include <stddef.h>
#include <stdint.h>

typedef struct dns_cache dns_cache;

typedef enum dns_cache_lookup_result
{
    DNS_CACHE_MISS = 0,
    DNS_CACHE_HIT,
    DNS_CACHE_EXPIRED
} dns_cache_lookup_result;

dns_cache *dns_cache_create(size_t capacity);
void dns_cache_destroy(dns_cache *c);

dns_cache_lookup_result dns_cache_lookup(dns_cache *c, const char *qname,
                                         uint32_t *ipv4_out, uint64_t now_ms);

int dns_cache_insert(dns_cache *c, const char *qname, uint32_t ipv4_be,
                     uint32_t ttl_sec, uint64_t now_ms);

/* Test / diagnostics: number of live entries. */
size_t dns_cache_size(const dns_cache *c);

#endif /* DNS_RELAY_DNS_CACHE_H */
