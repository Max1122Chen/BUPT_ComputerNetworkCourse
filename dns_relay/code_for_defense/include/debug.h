#ifndef DNS_RELAY_DEBUG_H
#define DNS_RELAY_DEBUG_H

#define DBG_L1 1
#define DBG_L2 2

void debug_init(int level);
int debug_get_level(void);
void debug_log(int level, const char *fmt, ...);

#endif
