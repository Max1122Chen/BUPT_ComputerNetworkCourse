#ifndef DNS_RELAY_PLATFORM_H
#define DNS_RELAY_PLATFORM_H

#include <stdint.h>

/* Winsock lifecycle and monotonic clock for pending timeouts. */

int platform_init(void);
void platform_cleanup(void);

/* Milliseconds since an arbitrary origin; monotonic on Windows. */
uint64_t platform_monotonic_ms(void);

#endif /* DNS_RELAY_PLATFORM_H */
