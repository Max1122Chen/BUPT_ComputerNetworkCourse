#ifndef DNS_RELAY_CONSTANTS_H
#define DNS_RELAY_CONSTANTS_H

/* Shared limits — see design-technical.md §7 */

#define DNS_UDP_BUF_SIZE           512
#define DNS_RELAY_PORT             53
#define DNS_RELAY_DEFAULT_UPSTREAM "202.106.0.20"
#define DNS_RELAY_DEFAULT_TABLE_PATH "dnsrelay.txt"
#define DNS_ID_MAP_TIMEOUT_MS      5000
#define DNS_ID_MAP_MAX_PENDING     256
#define DNS_ID_MAP_BUCKET_COUNT    256
#define RELAY_SELECT_MAX_MS        500

#define DNS_LOCAL_TTL              60

#define DNS_TABLE_DEFAULT_BUCKETS 2048

#define DNS_QNAME_MAX_LEN          256
#define DNS_NAME_JUMP_MAX          16

#endif /* DNS_RELAY_CONSTANTS_H */
