/*
 * Golden test-vector generator for the Python LwPKT implementation.
 *
 * Encodes packets with the reference C library (all features LWPKT_ON_DYNAMIC) for every
 * combination of the 7 runtime options and a few payloads, and prints them as JSON.
 *
 * Regenerate from the repository root:
 *
 *   cc -std=c11 -Ilwpkt/src/include -Ilibs/lwrb/src/include -Itests/dynamic \
 *      lwpkt/src/lwpkt/lwpkt.c libs/lwrb/src/lwrb/lwrb.c python/tests/c_vectors/gen_vectors.c \
 *      -o /tmp/gen_vectors && /tmp/gen_vectors > python/tests/c_vectors/vectors.json
 */
#include <stdio.h>
#include <string.h>
#include "lwpkt/lwpkt.h"
#include "lwrb/lwrb.h"

static lwpkt_t pkt;
static lwrb_t tx_rb, rx_rb;
static uint8_t tx_rb_data[1024], rx_rb_data[1024];

int
main(void) {
    static uint8_t long_data[200];
    const char* hello = "Hello World\r\n";
    const struct {
        const void* data;
        size_t len;
    } payloads[] = {
        {NULL, 0},
        {hello, 13},
        {long_data, sizeof(long_data)},
    };
    int first = 1;

    for (size_t i = 0; i < sizeof(long_data); ++i) {
        long_data[i] = (uint8_t)i;
    }

    lwrb_init(&tx_rb, tx_rb_data, sizeof(tx_rb_data));
    lwrb_init(&rx_rb, rx_rb_data, sizeof(rx_rb_data));
    lwpkt_init(&pkt, &tx_rb, &rx_rb);

    printf("[\n");
    for (unsigned opts = 0; opts < 128; ++opts) {
        uint8_t use_addr = !!(opts & 0x01), use_addr_ext = !!(opts & 0x02), use_flags = !!(opts & 0x04);
        uint8_t use_cmd = !!(opts & 0x08), use_cmd_ext = !!(opts & 0x10), use_crc = !!(opts & 0x20);
        uint8_t use_crc32 = !!(opts & 0x40);

        for (size_t p = 0; p < sizeof(payloads) / sizeof(payloads[0]); ++p) {
            uint32_t our_addr = 0x12345678UL, to = 0x87654321UL, flags = 0xACCE550FUL, cmd = 0x85542343UL;
            uint8_t b;

            if (!use_addr_ext) {
                our_addr &= 0xFFUL;
                to &= 0xFFUL;
            }
            if (!use_cmd_ext) {
                cmd &= 0xFFUL;
            }

            lwpkt_reset(&pkt);
            lwrb_reset(&tx_rb);
            lwpkt_set_addr(&pkt, our_addr);
            lwpkt_set_addr_enabled(&pkt, use_addr);
            lwpkt_set_addr_extended_enabled(&pkt, use_addr_ext);
            lwpkt_set_flags_enabled(&pkt, use_flags);
            lwpkt_set_cmd_enabled(&pkt, use_cmd);
            lwpkt_set_cmd_extended_enabled(&pkt, use_cmd_ext);
            lwpkt_set_crc_enabled(&pkt, use_crc);
            lwpkt_set_crc32_enabled(&pkt, use_crc32);

            if (lwpkt_write(&pkt, to, flags, cmd, payloads[p].data, payloads[p].len) != lwpktOK) {
                fprintf(stderr, "lwpkt_write failed: opts=%u payload=%u\n", opts, (unsigned)p);
                return 1;
            }

            printf("%s  {\"opts\": %u, \"payload\": %u, \"our_addr\": %lu, \"to\": %lu, \"flags\": %lu, "
                   "\"cmd\": %lu, \"packet\": \"",
                   first ? "" : ",\n", opts, (unsigned)p, (unsigned long)our_addr, (unsigned long)to,
                   (unsigned long)flags, (unsigned long)cmd);
            while (lwrb_read(&tx_rb, &b, 1) == 1) {
                printf("%02x", b);
            }
            printf("\"}");
            first = 0;
        }
    }
    printf("\n]\n");
    return 0;
}
