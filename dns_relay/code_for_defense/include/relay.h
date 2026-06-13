#ifndef DNS_RELAY_RELAY_H
#define DNS_RELAY_RELAY_H

#include "config.h"

int relay_run(const dns_relay_config *cfg);

void relay_request_stop(void);

#endif
