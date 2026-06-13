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

    memcpy(out, req_pkt, 12);
    memcpy(out + 12, req_pkt + 12, question_len);

    flags = (uint16_t)((req_pkt[2] << 8) | req_pkt[3]);

    flags |= 0x8000;
    flags &= ~0x0400;
    flags &= ~0x0200;
    flags = (uint16_t)((flags & 0xFFF0u) | (rcode & 0x0Fu));

    qdcount = (uint16_t)((req_pkt[4] << 8) | req_pkt[5]);

    out[2] = (uint8_t)((flags >> 8) & 0xFFu);
    out[3] = (uint8_t)(flags & 0xFFu);
    out[4] = (uint8_t)((qdcount >> 8) & 0xFFu);
    out[5] = (uint8_t)(qdcount & 0xFFu);

    out[6] = (uint8_t)((an_count >> 8) & 0xFFu);
    out[7] = (uint8_t)(an_count & 0xFFu);

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

    total_len = ans_off + 16;
    if (total_len > out_cap)
    {
        return -1;
    }

    if (dns_packet_copy_header_and_question(q, req_pkt, req_len, 1, 0, out, out_cap, out_len) != 0)
    {
        return -1;
    }

    ttl_be = (uint32_t)(DNS_LOCAL_TTL & 0xFFFFFFFFu);

    out[ans_off + 0] = 0xC0;
    out[ans_off + 1] = 0x0C;

    out[ans_off + 2] = 0;
    out[ans_off + 3] = 1;

    out[ans_off + 4] = 0;
    out[ans_off + 5] = 1;

    out[ans_off + 6] = (uint8_t)((ttl_be >> 24) & 0xFFu);
    out[ans_off + 7] = (uint8_t)((ttl_be >> 16) & 0xFFu);
    out[ans_off + 8] = (uint8_t)((ttl_be >> 8) & 0xFFu);
    out[ans_off + 9] = (uint8_t)(ttl_be & 0xFFu);

    out[ans_off + 10] = 0;
    out[ans_off + 11] = 4;

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

    return dns_packet_copy_header_and_question(q, req_pkt, req_len, 0, 3, out, out_cap, out_len);
}

static int skip_name_at(const uint8_t *pkt, size_t len, size_t offset, size_t *consumed)
{
    size_t pos = offset;
    int jumps = 0;
    size_t jump_return = 0;
    int jumped = 0;

    if (pkt == NULL || consumed == NULL || offset >= len)
    {
        return -1;
    }

    while (pos < len)
    {
        uint8_t label_len = pkt[pos];

        if ((label_len & 0xc0) == 0xc0)
        {
            if (pos + 1 >= len)
            {
                return -1;
            }
            if (jumps >= DNS_NAME_JUMP_MAX)
            {
                return -1;
            }

            if (!jumped)
            {
                jump_return = pos + 2;
                jumped = 1;
            }

            pos = (size_t)(((label_len & 0x3f) << 8) | pkt[pos + 1]);
            if (pos >= len)
            {
                return -1;
            }
            jumps++;
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

        pos += 1 + label_len;
    }

    return -1;
}

static size_t skip_questions(const uint8_t *pkt, size_t len, uint16_t qdcount)
{
    size_t offset = 12;
    uint16_t i;

    for (i = 0; i < qdcount; ++i)
    {
        size_t name_len;

        if (skip_name_at(pkt, len, offset, &name_len) != 0)
        {
            return 0;
        }
        offset += name_len;
        if (offset + 4 > len)
        {
            return 0;
        }
        offset += 4;
    }

    return offset;
}

static int scan_answers_for_a(const uint8_t *pkt, size_t len, size_t ans_offset,
                              uint16_t ancount, const char *want, const char *cname_target,
                              uint32_t *ipv4_out, uint32_t *ttl_out)
{
    size_t offset = ans_offset;
    uint16_t i;

    for (i = 0; i < ancount; ++i)
    {
        size_t name_consumed;
        int jumps = 0;
        char rname[256];
        uint16_t rtype;
        uint16_t rclass;
        uint32_t ttl;
        uint16_t rdlen;

        if (decode_name(pkt, len, offset, rname, sizeof(rname), &name_consumed, &jumps) != 0)
        {
            return -1;
        }

        offset += name_consumed;
        if (offset + 10 > len)
        {
            return -1;
        }

        rtype = (uint16_t)((pkt[offset] << 8) | pkt[offset + 1]);
        rclass = (uint16_t)((pkt[offset + 2] << 8) | pkt[offset + 3]);
        ttl = (uint32_t)((pkt[offset + 4] << 24) | (pkt[offset + 5] << 16) |
                         (pkt[offset + 6] << 8) | pkt[offset + 7]);
        rdlen = (uint16_t)((pkt[offset + 8] << 8) | pkt[offset + 9]);
        offset += 10;

        if (offset + rdlen > len)
        {
            return -1;
        }

        if (rtype == DNS_TYPE_A && rclass == DNS_CLASS_IN && rdlen == 4)
        {
            qname_to_lower(rname);
            if (strcmp(rname, want) == 0 ||
                (cname_target[0] != '\0' && strcmp(rname, cname_target) == 0))
            {
                memcpy(ipv4_out, pkt + offset, 4);
                *ttl_out = ttl;
                return 0;
            }
        }

        offset += rdlen;
    }

    return -1;
}

int dns_packet_extract_a_for_qname(const uint8_t *pkt, size_t len, const char *qname,
                                   uint32_t *ipv4_out, uint32_t *ttl_out)
{
    uint16_t qdcount;
    uint16_t ancount;
    size_t ans_offset;
    uint16_t i;
    char want[256];
    char cname_target[256];
    size_t offset;

    if (pkt == NULL || qname == NULL || ipv4_out == NULL || ttl_out == NULL || len < 12)
    {
        return -1;
    }

    if ((pkt[2] & 0x80) == 0)
    {
        return -1;
    }

    if ((pkt[3] & 0x0f) != 0)
    {
        return -1;
    }

    qdcount = (uint16_t)((pkt[4] << 8) | pkt[5]);
    ancount = (uint16_t)((pkt[6] << 8) | pkt[7]);
    if (qdcount < 1 || ancount < 1)
    {
        return -1;
    }

    if (strlen(qname) >= sizeof(want))
    {
        return -1;
    }

    strcpy(want, qname);
    qname_to_lower(want);
    cname_target[0] = '\0';

    ans_offset = skip_questions(pkt, len, qdcount);
    if (ans_offset == 0)
    {
        return -1;
    }

    offset = ans_offset;
    for (i = 0; i < ancount; ++i)
    {
        size_t name_consumed;
        int jumps = 0;
        char rname[256];
        uint16_t rtype;
        uint16_t rclass;
        uint16_t rdlen;
        size_t rd_consumed;

        if (decode_name(pkt, len, offset, rname, sizeof(rname), &name_consumed, &jumps) != 0)
        {
            return -1;
        }

        offset += name_consumed;
        if (offset + 10 > len)
        {
            return -1;
        }

        rtype = (uint16_t)((pkt[offset] << 8) | pkt[offset + 1]);
        rclass = (uint16_t)((pkt[offset + 2] << 8) | pkt[offset + 3]);
        rdlen = (uint16_t)((pkt[offset + 8] << 8) | pkt[offset + 9]);
        offset += 10;

        if (offset + rdlen > len)
        {
            return -1;
        }

        if (rtype == DNS_TYPE_CNAME && rclass == DNS_CLASS_IN)
        {
            qname_to_lower(rname);
            if (strcmp(rname, want) == 0)
            {
                jumps = 0;
                if (decode_name(pkt, len, offset, cname_target, sizeof(cname_target), &rd_consumed,
                                &jumps) == 0)
                {
                    qname_to_lower(cname_target);
                }
            }
        }

        offset += rdlen;
    }

    return scan_answers_for_a(pkt, len, ans_offset, ancount, want, cname_target, ipv4_out, ttl_out);
}
