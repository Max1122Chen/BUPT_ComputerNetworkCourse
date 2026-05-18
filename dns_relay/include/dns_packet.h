#ifndef DNS_RELAY_DNS_PACKET_H
#define DNS_RELAY_DNS_PACKET_H

#include <stddef.h>
#include <stdint.h>

#define DNS_TYPE_A   1
#define DNS_CLASS_IN 1

typedef struct dns_query_info
{
    uint16_t id;
    uint16_t qtype;
    uint16_t qclass;
    char qname[256];
    size_t qname_encoded_len;
    size_t question_end_offset;
} dns_query_info;

uint16_t dns_packet_get_id(const uint8_t *pkt);
void dns_packet_set_id(uint8_t *pkt, uint16_t id);

/* Parse standard query; qname lowercased. Returns 0 on success. */
int dns_packet_parse_query(const uint8_t *pkt, size_t len, dns_query_info *out);

#endif /* DNS_RELAY_DNS_PACKET_H */
