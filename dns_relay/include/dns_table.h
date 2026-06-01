#ifndef DNS_RELAY_DNS_TABLE_H
#define DNS_RELAY_DNS_TABLE_H

#include <stddef.h>
#include <stdint.h>

typedef struct dns_table dns_table;

typedef enum dns_table_result
{
    DNS_TABLE_MISS = 0,
    DNS_TABLE_HIT,
    DNS_TABLE_BLOCK /* 0.0.0.0 */
} dns_table_result;

dns_table *dns_table_create(size_t bucket_count);
void dns_table_destroy(dns_table *t);

/* Load "IP<space>domain" per line. Duplicate domains: later wins.
 * Returns number of loaded valid entries (>=0) or <0 on fatal error. */
int dns_table_load_file(dns_table *t, const char *path);

/* Lookup normalized lowercased qname (no trailing dot).
 * On HIT, writes ipv4_be (network byte order) into ipv4_out (optional). */
dns_table_result dns_table_lookup(const dns_table *t, const char *qname, uint32_t *ipv4_out);

#endif /* DNS_RELAY_DNS_TABLE_H */
