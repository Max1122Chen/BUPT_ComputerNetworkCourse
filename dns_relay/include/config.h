#ifndef DNS_RELAY_CONFIG_H
#define DNS_RELAY_CONFIG_H

#include <stdint.h>

typedef struct dns_relay_config
{
    char upstream_ip[46];
    uint16_t upstream_port;
    char table_path[512];
    int debug_level; /* 0 = silent, 1 = -d, 2 = -dd */
    int load_table;  /* Release 2+: load dnsrelay.txt */
} dns_relay_config;

void config_set_defaults(dns_relay_config *cfg);

/* Parse argv; returns 0 on success, <0 on error. */
int config_parse(int argc, char **argv, dns_relay_config *out);

#endif /* DNS_RELAY_CONFIG_H */
