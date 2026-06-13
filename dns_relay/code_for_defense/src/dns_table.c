#include "dns_table.h"

#include "dns_relay_constants.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

typedef struct dns_table_entry
{
    char *name;
    uint32_t ipv4_be;
    struct dns_table_entry *next;
} dns_table_entry;

struct dns_table
{
    dns_table_entry **buckets;
    size_t bucket_count;
};

static unsigned dns_table_hash(const char *s)
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

dns_table *dns_table_create(size_t bucket_count)
{
    dns_table *t = (dns_table *)calloc(1, sizeof(*t));
    if (t == NULL)
    {
        return NULL;
    }

    if (bucket_count == 0)
    {
        bucket_count = DNS_TABLE_DEFAULT_BUCKETS;
    }

    t->bucket_count = bucket_count;
    t->buckets = (dns_table_entry **)calloc(bucket_count, sizeof(t->buckets[0]));
    if (t->buckets == NULL)
    {
        free(t);
        return NULL;
    }
    return t;
}

void dns_table_destroy(dns_table *t)
{
    size_t i;

    if (t == NULL)
    {
        return;
    }

    for (i = 0; i < t->bucket_count; ++i)
    {
        dns_table_entry *e = t->buckets[i];
        while (e != NULL)
        {
            dns_table_entry *next = e->next;
            free(e->name);
            free(e);
            e = next;
        }
    }
    free(t->buckets);
    free(t);
}

int dns_table_load_file(dns_table *t, const char *path)
{
    FILE *fp = NULL;
    char line[512];
    int loaded = 0;

    if (t == NULL || path == NULL)
    {
        return -1;
    }

    fp = fopen(path, "r");
    if (fp == NULL)
    {
        debug_log(DBG_L1, "dns_table: open file failed: %s", path);
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char ip_str[64];
        char host[DNS_QNAME_MAX_LEN];
        uint32_t ipv4_be;
        struct in_addr addr;
        unsigned b;
        dns_table_entry *e;

        {
            char *p = line;
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            {
                ++p;
            }
            memmove(line, p, strlen(p) + 1);
        }

        {
            char *hash = strchr(line, '#');
            if (hash != NULL)
            {

                while (hash > line &&
                       (hash[-1] == ' ' || hash[-1] == '\t' || hash[-1] == '\r' || hash[-1] == '\n'))
                {
                    --hash;
                }
                *hash = '\0';
            }
        }

        if (line[0] == '\0')
        {
            continue;
        }

        if (line[0] == '#')
        {
            continue;
        }

        if (sscanf(line, "%63s %255s", ip_str, host) != 2)
        {
            continue;
        }

        qname_to_lower_inplace(host);

        if (strcmp(ip_str, "0.0.0.0") == 0)
        {
            ipv4_be = 0;
        }
        else
        {
            if (inet_pton(AF_INET, ip_str, &addr) != 1)
            {
                continue;
            }
            ipv4_be = addr.s_addr;
        }

        b = dns_table_hash(host) % (unsigned)t->bucket_count;

        for (e = t->buckets[b]; e != NULL; e = e->next)
        {
            if (strcmp(e->name, host) == 0)
            {
                e->ipv4_be = ipv4_be;
                goto next_line;
            }
        }

        e = (dns_table_entry *)calloc(1, sizeof(*e));
        if (e == NULL)
        {
            fclose(fp);
            return -1;
        }

        e->name = (char *)calloc(1, strlen(host) + 1);
        if (e->name == NULL)
        {
            free(e);
            fclose(fp);
            return -1;
        }
        strcpy(e->name, host);
        e->ipv4_be = ipv4_be;
        e->next = t->buckets[b];
        t->buckets[b] = e;
        loaded++;

    next_line:
        ;
    }

    fclose(fp);
    return loaded;
}

dns_table_result dns_table_lookup(const dns_table *t, const char *qname, uint32_t *ipv4_out)
{
    unsigned b;
    dns_table_entry *e;
    char tmp[DNS_QNAME_MAX_LEN];

    if (t == NULL || qname == NULL)
    {
        return DNS_TABLE_MISS;
    }

    if (strlen(qname) >= sizeof(tmp))
    {
        return DNS_TABLE_MISS;
    }

    strcpy(tmp, qname);
    qname_to_lower_inplace(tmp);
    b = dns_table_hash(tmp) % (unsigned)t->bucket_count;

    for (e = t->buckets[b]; e != NULL; e = e->next)
    {
        if (strcmp(e->name, tmp) == 0)
        {
            if (ipv4_out != NULL)
            {
                *ipv4_out = e->ipv4_be;
            }
            return e->ipv4_be == 0 ? DNS_TABLE_BLOCK : DNS_TABLE_HIT;
        }
    }

    return DNS_TABLE_MISS;
}
