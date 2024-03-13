#include <stdio.h>
#include "lwpkt/lwpkt.h"

/* LwPKT data */
static lwpkt_t pkt;
static lwrb_t pkt_tx_rb, pkt_rx_rb;
static uint8_t pkt_tx_rb_data[256], pkt_rx_rb_data[256];

/* Data to read and write */
static const char* data = "Hello World123456789\r\n";

static uint8_t
run_test(uint8_t conf_index, uint8_t use_addr, uint8_t use_addr_ext, uint8_t use_flags, uint8_t use_cmd,
         uint8_t use_crc, uint8_t use_crc32) {
    lwpktr_t res;
    uint8_t b;
    uint32_t our_addr = 0x12345678UL;
    uint32_t dest_addr = 0x87654321UL;
    uint32_t flags = 0xACCE550FUL;
    uint32_t cmd = 0x85UL;
    size_t data_len = strlen(data);

    /* Limit to low-nibble for non-extended addresses */
    if (!use_addr_ext) {
        our_addr &= 0xFFUL;
        dest_addr &= 0xFFUL;
    }

    /* Go to default state */
    lwpkt_reset(&pkt);
    lwrb_reset(pkt.tx_rb);
    lwrb_reset(pkt.rx_rb);

#if LWPKT_CFG_USE_ADDR
    if (use_addr) {
        lwpkt_set_addr(&pkt, our_addr);
    }
#endif
#if LWPKT_CFG_USE_ADDR == 2
    lwpkt_set_addr_enabled(&pkt, use_addr);
#endif /* LWPKT_CFG_USE_ADDR */
#if LWPKT_CFG_ADDR_EXTENDED == 2
    lwpkt_set_addr_extended_enabled(&pkt, use_addr_ext);
#endif /* LWPKT_CFG_ADDR_EXTENDED */

#if LWPKT_CFG_USE_FLAGS == 2
    lwpkt_set_flags_enabled(&pkt, use_flags);
#endif /* LWPKT_CFG_USE_FLAGS */
#if LWPKT_CFG_USE_CMD == 2
    lwpkt_set_cmd_enabled(&pkt, use_cmd);
#endif /* LWPKT_CFG_USE_CMD */
#if LWPKT_CFG_USE_CRC == 2
    lwpkt_set_crc_enabled(&pkt, use_crc);
#endif /* LWPKT_CFG_USE_CRC */
#if LWPKT_CFG_CRC32 == 2
    lwpkt_set_crc32_enabled(&pkt, use_crc32);
#endif /* LWPKT_CFG_CRC32 */

    /* Write the data */
    res = lwpkt_write(&pkt,
#if LWPKT_CFG_USE_ADDR
                      dest_addr, /* End address to whom to send */
#endif                           /* LWPKT_CFG_USE_ADDR */
#if LWPKT_CFG_USE_FLAGS
                      flags,
#endif /* LWPKT_CFG_USE_FLAGS */
#if LWPKT_CFG_USE_CMD
                      cmd,             /* Command type */
#endif                                 /* LWPKT_CFG_USE_CMD */
                      data, data_len); /* Length of data and actual data */

    /* */
    printf("--\r\n Conf: %u, use_addr: %u, use_addr_ext: %u, use_flags: %u, use_cmd: %u,"
           " use_crc: %u, use_crc32: %u\r\n",
           (unsigned)conf_index, (unsigned)use_addr, (unsigned)use_addr_ext, (unsigned)use_flags, (unsigned)use_cmd,
           (unsigned)use_crc, (unsigned)use_crc32);

    /* Copy data from TX to RX buffer -> immitate receive operation */
    printf("LwRB len: %u, content: ", (unsigned)lwrb_get_full(&pkt_tx_rb));
    while (lwrb_read(&pkt_tx_rb, &b, 1) == 1) {
        printf("0x%02X,", (unsigned)b);
        lwrb_write(&pkt_rx_rb, &b, 1);
    }
    printf("\r\n");

    /* Now read and process packet */
    res = lwpkt_read(&pkt);

    if (res == lwpktVALID) {
        /* Print debug messages for packet */
        if (0) {
#if LWPKT_CFG_USE_ADDR
        } else if (use_addr && our_addr != lwpkt_get_from_addr(&pkt)) {
            printf("receive address mismatch\r\n");
        } else if (use_addr && dest_addr != lwpkt_get_to_addr(&pkt)) {
            printf("destination address mismatch\r\n");
#endif /* LWPKT_CFG_USE_ADDR */
#if LWPKT_CFG_USE_FLAGS
        } else if (use_flags && flags != lwpkt_get_flags(&pkt)) {
            printf("flags mismatch\r\n");
#endif /* LWPKT_CFG_USE_FLAGS */
#if LWPKT_CFG_USE_CMD
        } else if (use_cmd && cmd != lwpkt_get_cmd(&pkt)) {
            printf("command mismatch\r\n");
#endif /* LWPKT_CFG_USE_CMD */
        } else if (data_len != lwpkt_get_data_len(&pkt)) {
            printf("data len mismatch\r\n");
        } else {
            uint8_t ok = 1;
            const uint8_t* ptr = lwpkt_get_data(&pkt);
            for (size_t i = 0; i < data_len; ++i) {
                if (ptr[i] != data[i]) {
                    printf("Data mismatch at index %u\r\n", (unsigned)i);
                    ok = 0;
                    break;
                }
            }
            if (ok) {
                printf("Test OK\r\n");
            }
        }
    } else if (res == lwpktINPROG) {
        printf("Packet is still in progress, did not receive yet all bytes..\r\n");
    } else {
        printf("Packet is not valid!\r\n");
    }
    printf("--\r\n");
    return 0;
}

/**
 * \brief           LwPKT example code
 */
void
test_lwpkt(void) {
    printf("---\r\nLwPKT test.\r\n\r\n");

    /* Setup the lib */
    lwrb_init(&pkt_tx_rb, pkt_tx_rb_data, sizeof(pkt_tx_rb_data));
    lwrb_init(&pkt_rx_rb, pkt_rx_rb_data, sizeof(pkt_rx_rb_data));
    lwpkt_init(&pkt, &pkt_tx_rb, &pkt_rx_rb);

    /* Try */
    for (size_t i = 0; i < 1 << 6; ++i) {
        run_test(i + 1, !!(i & 0x01), !!(i & 0x02), !!(i & 0x04), !!(i & 0x08), !!(i & 0x10), !!(i & 0x20));
    }
}
