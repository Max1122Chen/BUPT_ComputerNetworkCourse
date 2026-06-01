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

/* Build local A response:
 * - Uses req header + Question from req_pkt
 * - Sets flags: QR=1, AA=0
 * - Sets RCODE=0 and Answer section to one A record with ipv4_be
 * Returns 0 on success, <0 on error (out_cap insufficient). */
int dns_packet_build_a_response(const dns_query_info *q, const uint8_t *req_pkt,
                                 size_t req_len, uint32_t ipv4_be,
                                 uint8_t *out, size_t out_cap,
                                 size_t *out_len);

/* Build local NXDOMAIN response (no Answer):
 * - Uses req header + Question from req_pkt
 * - Sets flags: QR=1, AA=0
 * - Sets RCODE=3 and ANCOUNT=0
 * Returns 0 on success, <0 on error. */
int dns_packet_build_nxdomain(const dns_query_info *q, const uint8_t *req_pkt,
                               size_t req_len, uint8_t *out,
                               size_t out_cap, size_t *out_len);

#endif /* DNS_RELAY_DNS_PACKET_H */
