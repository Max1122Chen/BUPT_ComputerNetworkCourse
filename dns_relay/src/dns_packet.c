#include "dns_packet.h"

#include "dns_relay_constants.h"

#include <ctype.h>
#include <string.h>

uint16_t dns_packet_get_id(const uint8_t *pkt)
{
    if (pkt == NULL)
    {
        return 0;
    }
    return (uint16_t)((pkt[0] << 8) | pkt[1]);
}

void dns_packet_set_id(uint8_t *pkt, uint16_t id)
{
    if (pkt == NULL)
    {
        return;
    }
    pkt[0] = (uint8_t)((id >> 8) & 0xff);
    pkt[1] = (uint8_t)(id & 0xff);
}

static int decode_name(const uint8_t *pkt, size_t len, size_t offset, char *out,
                       size_t out_cap, size_t *consumed, int *jump_count)
{
    size_t pos = offset;
    size_t out_pos = 0;
    int jumped = 0;
    size_t jump_return = 0;

    if (out == NULL || consumed == NULL || jump_count == NULL || out_cap == 0)
    {
        return -1;
    }

    out[0] = '\0';

    while (pos < len)
    {
        uint8_t label_len = pkt[pos];

        if ((label_len & 0xc0) == 0xc0)
        {
            uint16_t ptr;

            if (pos + 1 >= len)
            {
                return -1;
            }
            if (*jump_count >= DNS_NAME_JUMP_MAX)
            {
                return -1;
            }

            ptr = (uint16_t)(((label_len & 0x3f) << 8) | pkt[pos + 1]);
            if (ptr >= len)
            {
                return -1;
            }

            if (!jumped)
            {
                jump_return = pos + 2;
                jumped = 1;
            }

            (*jump_count)++;
            pos = ptr;
            continue;
        }

        if (label_len == 0)
        {
            if (!jumped)
            {
                *consumed = pos + 1 - offset;
            }
            else
            {
                *consumed = jump_return - offset;
            }
            return 0;
        }

        if (label_len > 63 || pos + 1 + label_len > len)
        {
            return -1;
        }

        if (out_pos > 0)
        {
            if (out_pos + 1 >= out_cap)
            {
                return -1;
            }
            out[out_pos++] = '.';
        }

        if (out_pos + label_len >= out_cap)
        {
            return -1;
        }

        memcpy(out + out_pos, pkt + pos + 1, label_len);
        out_pos += label_len;
        out[out_pos] = '\0';
        pos += 1 + label_len;
    }

    return -1;
}

static void qname_to_lower(char *name)
{
    size_t i;

    for (i = 0; name[i] != '\0'; ++i)
    {
        name[i] = (char)tolower((unsigned char)name[i]);
    }
}

int dns_packet_parse_query(const uint8_t *pkt, size_t len, dns_query_info *out)
{
    uint16_t qdcount;
    size_t name_consumed;
    size_t qend;
    int jumps = 0;

    if (pkt == NULL || out == NULL || len < 12)
    {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    out->id = dns_packet_get_id(pkt);
    qdcount = (uint16_t)((pkt[4] << 8) | pkt[5]);
    if (qdcount < 1)
    {
        return -1;
    }

    if (decode_name(pkt, len, 12, out->qname, sizeof(out->qname), &name_consumed, &jumps) != 0)
    {
        return -1;
    }

    out->qname_encoded_len = name_consumed;
    qend = 12 + name_consumed;
    if (qend + 4 > len)
    {
        return -1;
    }

    out->qtype = (uint16_t)((pkt[qend] << 8) | pkt[qend + 1]);
    out->qclass = (uint16_t)((pkt[qend + 2] << 8) | pkt[qend + 3]);
    out->question_end_offset = qend + 4 - 12;

    qname_to_lower(out->qname);
    return 0;
}

static int dns_packet_copy_header_and_question(const dns_query_info *q,
                                                 const uint8_t *req_pkt,
                                                 size_t req_len,
                                                 uint16_t an_count,
                                                 uint8_t rcode,
                                                 uint8_t *out,
                                                 size_t out_cap,
                                                 size_t *out_len)
{
    uint16_t flags;
    uint16_t qdcount;
    size_t question_len;
    size_t total_len;

    if (q == NULL || req_pkt == NULL || out == NULL || out_len == NULL)
    {
        return -1;
    }
    if (req_len < 12)
    {
        return -1;
    }
    if (q->question_end_offset == 0)
    {
        return -1;
    }

    question_len = q->question_end_offset;
    if (12 + question_len > req_len)
    {
        return -1;
    }

    total_len = 12 + question_len;
    if (total_len > out_cap)
    {
        return -1;
    }

    /* Copy header and Question section verbatim. */
    memcpy(out, req_pkt, 12);
    memcpy(out + 12, req_pkt + 12, question_len);

    flags = (uint16_t)((req_pkt[2] << 8) | req_pkt[3]);
    /* QR=1 (response), AA=0, TC=0. Keep RD/RA fields as-is. */
    flags |= 0x8000;
    flags &= ~0x0400; /* AA */
    flags &= ~0x0200; /* TC */
    flags = (uint16_t)((flags & 0xFFF0u) | (rcode & 0x0Fu));

    qdcount = (uint16_t)((req_pkt[4] << 8) | req_pkt[5]);

    /* Header counts. */
    out[2] = (uint8_t)((flags >> 8) & 0xFFu);
    out[3] = (uint8_t)(flags & 0xFFu);
    out[4] = (uint8_t)((qdcount >> 8) & 0xFFu);
    out[5] = (uint8_t)(qdcount & 0xFFu);

    out[6] = (uint8_t)((an_count >> 8) & 0xFFu);
    out[7] = (uint8_t)(an_count & 0xFFu);

    /* NSCOUNT=0, ARCOUNT=0. */
    out[8] = 0;
    out[9] = 0;
    out[10] = 0;
    out[11] = 0;

    *out_len = total_len;
    return 0;
}

int dns_packet_build_a_response(const dns_query_info *q, const uint8_t *req_pkt,
                                 size_t req_len, uint32_t ipv4_be,
                                 uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t question_len;
    size_t ans_off;
    size_t total_len;
    uint32_t ttl_be;

    if (q == NULL || req_pkt == NULL || out == NULL || out_len == NULL)
    {
        return -1;
    }

    question_len = q->question_end_offset;
    ans_off = 12 + question_len;

    /* A record (NAME ptr + TYPE + CLASS + TTL + RDLENGTH + RDATA). */
    total_len = ans_off + 16;
    if (total_len > out_cap)
    {
        return -1;
    }

    if (dns_packet_copy_header_and_question(q, req_pkt, req_len, 1, 0, out, out_cap, out_len) != 0)
    {
        return -1;
    }

    /* TTL and RDATA are written in network byte order. */
    ttl_be = (uint32_t)(DNS_LOCAL_TTL & 0xFFFFFFFFu);

    /* NAME: pointer to QNAME at offset 12 (0xC00C). */
    out[ans_off + 0] = 0xC0;
    out[ans_off + 1] = 0x0C;

    /* TYPE=A (1). */
    out[ans_off + 2] = 0;
    out[ans_off + 3] = 1;

    /* CLASS=IN (1). */
    out[ans_off + 4] = 0;
    out[ans_off + 5] = 1;

    /* TTL. */
    out[ans_off + 6] = (uint8_t)((ttl_be >> 24) & 0xFFu);
    out[ans_off + 7] = (uint8_t)((ttl_be >> 16) & 0xFFu);
    out[ans_off + 8] = (uint8_t)((ttl_be >> 8) & 0xFFu);
    out[ans_off + 9] = (uint8_t)(ttl_be & 0xFFu);

    /* RDLENGTH=4. */
    out[ans_off + 10] = 0;
    out[ans_off + 11] = 4;

    /* RDATA=IPv4 (4 bytes). */
    /* ipv4_be is in network byte order stored in in_addr.s_addr.
     * On little-endian hosts, using shifts would swap byte order.
     * Copy the raw 4 bytes to the wire. */
    memcpy(out + ans_off + 12, &ipv4_be, 4);

    *out_len = total_len;
    return 0;
}

int dns_packet_build_nxdomain(const dns_query_info *q, const uint8_t *req_pkt,
                               size_t req_len, uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (q == NULL || req_pkt == NULL || out == NULL || out_len == NULL)
    {
        return -1;
    }

    /* ANCOUNT=0, RCODE=3 for NXDOMAIN. */
    return dns_packet_copy_header_and_question(q, req_pkt, req_len, 0, 3, out, out_cap, out_len);
}
