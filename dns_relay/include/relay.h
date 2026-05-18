#ifndef DNS_RELAY_RELAY_H
#define DNS_RELAY_RELAY_H

#include "config.h"

/* Blocking event loop until platform stop flag (Ctrl+C). Returns 0 on clean exit. */
int relay_run(const dns_relay_config *cfg);

/* Called from platform to end relay_run. */
void relay_request_stop(void);

#endif /* DNS_RELAY_RELAY_H */
