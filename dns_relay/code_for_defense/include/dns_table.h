#ifndef DNS_RELAY_DNS_TABLE_H
#define DNS_RELAY_DNS_TABLE_H

#include <stddef.h>
#include <stdint.h>

typedef struct dns_table dns_table;

typedef enum dns_table_result
{
    DNS_TABLE_MISS = 0,
    DNS_TABLE_HIT,
    DNS_TABLE_BLOCK
} dns_table_result;

dns_table *dns_table_create(size_t bucket_count);
void dns_table_destroy(dns_table *t);

int dns_table_load_file(dns_table *t, const char *path);

dns_table_result dns_table_lookup(const dns_table *t, const char *qname, uint32_t *ipv4_out);

#endif
