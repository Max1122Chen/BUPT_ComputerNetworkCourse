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
