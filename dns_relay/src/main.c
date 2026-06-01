#include "config.h"
#include "debug.h"
#include "platform.h"
#include "relay.h"

#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <windows.h>

static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type)
{
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT)
    {
        relay_request_stop();
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char **argv)
{
    dns_relay_config cfg;
    int r;

    if (config_parse(argc, argv, &cfg) != 0)
    {
        config_print_help(argc > 0 ? argv[0] : "dnsrelay");
        return 1;
    }

    if (cfg.show_help)
    {
        config_print_help(argc > 0 ? argv[0] : "dnsrelay");
        return 0;
    }

    debug_init(cfg.debug_level);

    if (platform_init() != 0)
    {
        fprintf(stderr, "dnsrelay: Winsock init failed\n");
        return 1;
    }

    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    r = relay_run(&cfg);

    platform_cleanup();
    return r == 0 ? 0 : 1;
}
