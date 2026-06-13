#ifndef DNS_RELAY_PLATFORM_H
#define DNS_RELAY_PLATFORM_H

#include <stdint.h>

int platform_init(void);
void platform_cleanup(void);

uint64_t platform_monotonic_ms(void);

#endif
