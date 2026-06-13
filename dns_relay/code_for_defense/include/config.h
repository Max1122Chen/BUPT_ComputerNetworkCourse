#ifndef DNS_RELAY_CONFIG_H
#define DNS_RELAY_CONFIG_H

#include <stdint.h>

typedef struct dns_relay_config
{
    char upstream_ip[46];
    uint16_t upstream_port;
    char table_path[512];
    int debug_level;
    int load_table;
    int show_help;
} dns_relay_config;

void config_set_defaults(dns_relay_config *cfg);

int config_parse(int argc, char **argv, dns_relay_config *out);

void config_print_help(const char *prog_name);

#endif
