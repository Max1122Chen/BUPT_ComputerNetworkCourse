#include "config.h"

#include "dns_relay_constants.h"

#include <stdio.h>
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

static int dnsrelay_parse_ipv4(const char *s)
{
    struct in_addr addr;

    if (s == NULL)
    {
        return 0;
    }
    return inet_pton(AF_INET, s, &addr) == 1;
}

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
    cfg->load_table = 1; /* Release 2+: load dnsrelay.txt by default */
    cfg->show_help = 0;
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
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            out->show_help = 1;
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
        else if (dnsrelay_parse_ipv4(argv[i]))
        {
            strncpy(out->upstream_ip, argv[i], sizeof(out->upstream_ip) - 1);
        }
        else
        {
            strncpy(out->table_path, argv[i], sizeof(out->table_path) - 1);
            /* Use this path for the static table (Release 2+). */
        }
    }

    (void)seen_d;
    return 0;
}

void config_print_help(const char *prog_name)
{
    const char *name = prog_name != NULL ? prog_name : "dnsrelay";

    fprintf(stderr,
            "Usage: %s [-h|--help] [-d|-dd] [upstream-ip] [config-file]\n"
            "\n"
            "Options:\n"
            "  -h, --help     Show this help message and exit\n"
            "  -d             Enable level-1 debug logs\n"
            "  -dd            Enable level-2 debug logs (more verbose)\n"
            "\n"
            "Arguments:\n"
            "  upstream-ip    Upstream DNS IPv4 address (default: %s)\n"
            "  config-file    Static DNS table file path (default: %s)\n"
            "\n"
            "Examples:\n"
            "  %s\n"
            "  %s -d 220.181.111.1\n"
            "  %s -dd 220.181.111.232 dnsrelay.txt\n",
            name,
            DNS_RELAY_DEFAULT_UPSTREAM,
            DNS_RELAY_DEFAULT_TABLE_PATH,
            name,
            name,
            name);
}
