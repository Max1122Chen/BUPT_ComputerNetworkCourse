/*
 * Unit tests for Release 2 modules (no live DNS/network required).
 * Run: build target dnsrelay_test, then ./dnsrelay_test
 */

#include "config.h"
#include "dns_packet.h"
#include "dns_table.h"
#include "id_map.h"
#include "net_util.h"
#include "platform.h"
#include "test_framework.h"

#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

static int g_fail_before;

static void run_one(void (*fn)(void), const char *name)
{
    g_fail_before = g_tests_failed;
    printf("  %s ... ", name);
    fflush(stdout);
    fn();
    if (g_tests_failed == g_fail_before)
    {
        printf("OK\n");
    }
    else
    {
        printf("FAIL\n");
    }
    g_tests_run++;
}

static void test_dns_packet_id_roundtrip(void)
{
    uint8_t pkt[12] = {0};
    dns_packet_set_id(pkt, 0xabcd);
    TEST_ASSERT(dns_packet_get_id(pkt) == 0xabcd);
}

static void test_dns_packet_parse_simple(void)
{
    /* Query: www.example.com A IN, ID=0x1234 */
    uint8_t pkt[] = {
        0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x03, 'w', 'w', 'w', 0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'c', 'o', 'm', 0x00,
        0x00, 0x01, 0x00, 0x01};
    dns_query_info info;

    TEST_ASSERT(dns_packet_parse_query(pkt, sizeof(pkt), &info) == 0);
    TEST_ASSERT(info.id == 0x1234);
    TEST_ASSERT(info.qtype == DNS_TYPE_A);
    TEST_ASSERT(info.qclass == DNS_CLASS_IN);
    TEST_ASSERT(strcmp(info.qname, "www.example.com") == 0);
}

static void test_id_map_insert_take(void)
{
    id_map *m = id_map_create();
    struct sockaddr_storage client;
    struct sockaddr_in *in = (struct sockaddr_in *)&client;
    uint16_t uid = 0;
    id_map_entry *e;

    memset(&client, 0, sizeof(client));
    in->sin_family = AF_INET;
    in->sin_port = htons(54000);
    inet_pton(AF_INET, "192.168.1.10", &in->sin_addr);

    TEST_ASSERT(m != NULL);
    TEST_ASSERT(id_map_insert(m, 0x1111, &client, sizeof(*in), "www.test.com", &uid) == 0);
    TEST_ASSERT(uid != 0);
    TEST_ASSERT(id_map_pending_count(m) == 1);

    e = id_map_take(m, uid);
    TEST_ASSERT(e != NULL);
    TEST_ASSERT(e->client_id == 0x1111);
    TEST_ASSERT(strcmp(e->qname, "www.test.com") == 0);
    TEST_ASSERT(id_map_pending_count(m) == 0);
    free(e);
    id_map_destroy(m);
}

static void test_id_map_expire(void)
{
    id_map *m = id_map_create();
    struct sockaddr_storage client;
    struct sockaddr_in *in = (struct sockaddr_in *)&client;
    uint16_t uid = 0;
    uint64_t far_future;

    memset(&client, 0, sizeof(client));
    in->sin_family = AF_INET;
    in->sin_port = htons(54001);
    inet_pton(AF_INET, "192.168.1.11", &in->sin_addr);

    TEST_ASSERT(id_map_insert(m, 0x2222, &client, sizeof(*in), NULL, &uid) == 0);
    far_future = platform_monotonic_ms() + 60000;
    TEST_ASSERT(id_map_expire(m, far_future) == 1);
    TEST_ASSERT(id_map_pending_count(m) == 0);
    id_map_destroy(m);
}

static void test_config_defaults(void)
{
    dns_relay_config cfg;
    config_set_defaults(&cfg);
    TEST_ASSERT(cfg.debug_level == 0);
    TEST_ASSERT(cfg.load_table == 1);
    TEST_ASSERT(strcmp(cfg.upstream_ip, "202.106.0.20") == 0);
}

static void test_config_parse_filename(void)
{
    dns_relay_config cfg;
    char *argvv[] = {
        "dnsrelay",
        "-d",
        "220.181.111.1",
        "dnsrelay.txt"
    };

    TEST_ASSERT(config_parse(4, argvv, &cfg) == 0);
    TEST_ASSERT(cfg.debug_level == 1);
    TEST_ASSERT(strcmp(cfg.upstream_ip, "220.181.111.1") == 0);
    TEST_ASSERT(strcmp(cfg.table_path, "dnsrelay.txt") == 0);
    TEST_ASSERT(cfg.load_table == 1);
}

static void test_dns_table_load_and_lookup(void)
{
    dns_table *t = dns_table_create(64);
    int loaded;
    uint32_t ipv4_be = 0;
    dns_table_result r;

    TEST_ASSERT(t != NULL);

    loaded = dns_table_load_file(t, "samples/dnsrelay-minimal.txt");
    TEST_ASSERT(loaded >= 4);

    r = dns_table_lookup(t, "test1", &ipv4_be);
    TEST_ASSERT(r == DNS_TABLE_HIT);

    /* 11.111.11.111 in network byte order */
    {
        struct in_addr addr;
        inet_pton(AF_INET, "11.111.11.111", &addr);
        TEST_ASSERT(ipv4_be == addr.s_addr);
    }

    r = dns_table_lookup(t, "test0", &ipv4_be);
    TEST_ASSERT(r == DNS_TABLE_BLOCK);

    r = dns_table_lookup(t, "not-in-table.example", &ipv4_be);
    TEST_ASSERT(r == DNS_TABLE_MISS);

    dns_table_destroy(t);
}

static void test_dns_packet_build_a_response(void)
{
    const uint8_t req_pkt[] = {
        0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x03, 'w', 'w', 'w', 0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'c', 'o', 'm', 0x00,
        0x00, 0x01, 0x00, 0x01
    };
    uint8_t out[512];
    dns_query_info qinfo;
    size_t out_len;
    uint32_t ipv4_be;

    /* Same query used in test_dns_packet_parse_simple. */
    TEST_ASSERT(dns_packet_parse_query(req_pkt, sizeof(req_pkt), &qinfo) == 0);
    TEST_ASSERT(qinfo.qtype == DNS_TYPE_A);
    TEST_ASSERT(qinfo.qclass == DNS_CLASS_IN);

    {
        struct in_addr addr;
        inet_pton(AF_INET, "11.111.11.111", &addr);
        ipv4_be = addr.s_addr;
    }

    TEST_ASSERT(dns_packet_build_a_response(&qinfo, req_pkt, sizeof(req_pkt), ipv4_be,
                                             out, sizeof(out), &out_len) == 0);

    TEST_ASSERT(out_len == 12 + qinfo.question_end_offset + 16);

    /* QR=1 */
    TEST_ASSERT((out[2] & 0x80) != 0);
    /* AA=0 => clear bit 0x04 in high byte of flags */
    TEST_ASSERT((out[2] & 0x04) == 0);
    /* RCODE=0 */
    TEST_ASSERT((out[3] & 0x0F) == 0);

    /* ANCOUNT=1 */
    TEST_ASSERT(((uint16_t)out[6] << 8 | out[7]) == 1);

    /* Answer section */
    {
        size_t ans_off = 12 + qinfo.question_end_offset;
        TEST_ASSERT(out[ans_off] == 0xC0 && out[ans_off + 1] == 0x0C);
        TEST_ASSERT(((uint16_t)out[ans_off + 2] << 8 | out[ans_off + 3]) == 1);
        TEST_ASSERT(((uint16_t)out[ans_off + 4] << 8 | out[ans_off + 5]) == 1);
        TEST_ASSERT(out[ans_off + 10] == 0 && out[ans_off + 11] == 4);
        {
            uint8_t expected[4];
            memcpy(expected, &ipv4_be, 4);
            TEST_ASSERT(out[ans_off + 12] == expected[0]);
            TEST_ASSERT(out[ans_off + 13] == expected[1]);
            TEST_ASSERT(out[ans_off + 14] == expected[2]);
            TEST_ASSERT(out[ans_off + 15] == expected[3]);
        }
    }
}

static void test_dns_packet_build_nxdomain(void)
{
    const uint8_t req_pkt[] = {
        0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x03, 'w', 'w', 'w', 0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'c', 'o', 'm', 0x00,
        0x00, 0x01, 0x00, 0x01
    };
    uint8_t out[512];
    dns_query_info qinfo;
    size_t out_len;

    TEST_ASSERT(dns_packet_parse_query(req_pkt, sizeof(req_pkt), &qinfo) == 0);

    TEST_ASSERT(dns_packet_build_nxdomain(&qinfo, req_pkt, sizeof(req_pkt), out, sizeof(out), &out_len) == 0);
    TEST_ASSERT(out_len == 12 + qinfo.question_end_offset);

    TEST_ASSERT((out[2] & 0x80) != 0);
    TEST_ASSERT((out[2] & 0x04) == 0);
    TEST_ASSERT((out[3] & 0x0F) == 3);

    TEST_ASSERT(((uint16_t)out[6] << 8 | out[7]) == 0);
}

static void test_net_util_addr_equals(void)
{
    struct sockaddr_storage a, b;
    struct sockaddr_in *ina = (struct sockaddr_in *)&a;
    struct sockaddr_in *inb = (struct sockaddr_in *)&b;

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    ina->sin_family = AF_INET;
    inb->sin_family = AF_INET;
    ina->sin_port = htons(53);
    inb->sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &ina->sin_addr);
    inet_pton(AF_INET, "8.8.8.8", &inb->sin_addr);
    TEST_ASSERT(net_util_addr_equals(&a, sizeof(*ina), &b, sizeof(*inb)) == 1);

    inet_pton(AF_INET, "1.1.1.1", &inb->sin_addr);
    TEST_ASSERT(net_util_addr_equals(&a, sizeof(*ina), &b, sizeof(*inb)) == 0);
}

int main(void)
{
    printf("dnsrelay_test\n");

    if (platform_init() != 0)
    {
        fprintf(stderr, "platform_init failed\n");
        return 1;
    }

    run_one(test_dns_packet_id_roundtrip, "dns_packet_id");
    run_one(test_dns_packet_parse_simple, "dns_packet_parse");
    run_one(test_id_map_insert_take, "id_map_insert_take");
    run_one(test_id_map_expire, "id_map_expire");
    run_one(test_config_defaults, "config_defaults");
    run_one(test_config_parse_filename, "config_parse_filename");
    run_one(test_dns_table_load_and_lookup, "dns_table_load_lookup");
    run_one(test_dns_packet_build_a_response, "dns_packet_build_a");
    run_one(test_dns_packet_build_nxdomain, "dns_packet_build_nxdomain");
    run_one(test_net_util_addr_equals, "net_util_addr_equals");

    platform_cleanup();

    printf("Results: %d run, %d failed\n", g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
