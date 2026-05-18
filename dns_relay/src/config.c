#include "config.h"

#include "dns_relay_constants.h"

#include <stdio.h>
#include <string.h>

void config_set_defaults(dns_relay_config *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->upstream_ip, DNS_RELAY_DEFAULT_UPSTREAM, sizeof(cfg->upstream_ip) - 1);
    cfg->upstream_port = DNS_RELAY_PORT;
    strncpy(cfg->table_path, DNS_RELAY_DEFAULT_TABLE_PATH, sizeof(cfg->table_path) - 1);
    cfg->debug_level = 0;
    cfg->load_table = 0; /* Release 1: no static table */
}

int config_parse(int argc, char **argv, dns_relay_config *out)
{
    int i;
    int seen_d = 0;

    if (out == NULL)
    {
        return -1;
    }

    config_set_defaults(out);

    for (i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-d") == 0)
        {
            out->debug_level = 1;
            seen_d = 1;
        }
        else if (strcmp(argv[i], "-dd") == 0)
        {
            out->debug_level = 2;
            seen_d = 1;
        }
        else if (argv[i][0] == '-')
        {
            fprintf(stderr, "dnsrelay: unknown option: %s\n", argv[i]);
            return -1;
        }
        else if (strchr(argv[i], '.') != NULL && strchr(argv[i], '/') == NULL &&
                 strchr(argv[i], '\\') == NULL)
        {
            /* Heuristic: looks like an IPv4 address */
            strncpy(out->upstream_ip, argv[i], sizeof(out->upstream_ip) - 1);
        }
        else
        {
            strncpy(out->table_path, argv[i], sizeof(out->table_path) - 1);
            /* Release 2 will set load_table when file is used */
        }
    }

    (void)seen_d;
    return 0;
}
