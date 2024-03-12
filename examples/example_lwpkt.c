#include <stdio.h>
#include "lwpkt/lwpkt.h"

/* LwPKT data */
static lwpkt_t pkt;
static lwrb_t pkt_tx_rb, pkt_rx_rb;
static uint8_t pkt_tx_rb_data[64], pkt_rx_rb_data[64];

/* Data to read and write */
static const char* data = "Hello World\r\n";

/**
 * \brief           LwPKT example code
 */
void
example_lwpkt(void) {
    lwpktr_t res;
    uint8_t b;

    printf("---\r\nLwPKT default example..\r\n\r\n");

    /* 
     * Initialize both ring buffers, for TX and RX operations
     *
     * Initialize LwPKT and link buffers together
     */
    lwrb_init(&pkt_tx_rb, pkt_tx_rb_data, sizeof(pkt_tx_rb_data));
    lwrb_init(&pkt_rx_rb, pkt_rx_rb_data, sizeof(pkt_rx_rb_data));
    lwpkt_init(&pkt, &pkt_tx_rb, &pkt_rx_rb);

#if LWPKT_CFG_USE_ADDR
    /* Set device address (if feature enabled) */
    lwpkt_set_addr(&pkt, 0x12);
#endif /* LWPKT_CFG_USE_ADDR */

    /* 
     * Write packet to the TX ringbuffer,
     * act as device wants to send some data
     */
    res = lwpkt_write(&pkt,
#if LWPKT_CFG_USE_ADDR
                      0x11, /* End address to whom to send */
#endif                      /* LWPKT_CFG_USE_ADDR */
#if LWPKT_CFG_USE_FLAGS
                      0x12345678,
#endif /* LWPKT_CFG_USE_FLAGS */
#if LWPKT_CFG_USE_CMD
                      0x85,                /* Command type */
#endif                                     /* LWPKT_CFG_USE_CMD */
                      data, strlen(data)); /* Length of data and actual data */

    /*
     * LwPKT wrote data to pkt_tx_rb ringbuffer
     * Now actually transmit data over your interface
     * (USART for example, ...)
     */

    /*
     * For the purpose of this example, application will
     * fake data transmission by doing reading from TX buffer
     * and writing it to RX buffer
     */
    printf("Tx RB content len: %u, content: ", (unsigned)lwrb_get_full(&pkt_tx_rb));
    while (lwrb_read(&pkt_tx_rb, &b, 1) == 1) {
        printf("0x%02X, ", (unsigned)b);
        lwrb_write(&pkt_rx_rb, &b, 1);
    }
    printf("\r\n");

    /*
     * Here we have our data in RX buffer
     * means we received data over network interface
     */

    /* Now read and process packet */
    res = lwpkt_read(&pkt);

    if (res == lwpktVALID) {
        size_t len;

        /* Packet is valid */
        printf("Packet is valid!\r\n");

        /* Print debug messages for packet */
#if LWPKT_CFG_USE_ADDR
        printf("Packet from: 0x%08X\r\n", (unsigned)lwpkt_get_from_addr(&pkt));
        printf("Packet to: 0x%08X\r\n", (unsigned)lwpkt_get_to_addr(&pkt));
#endif /* LWPKT_CFG_USE_ADDR */
#if LWPKT_CFG_USE_FLAGS
        printf("Packet flags: 0x%08X\r\n", (unsigned)lwpkt_get_flags(&pkt));
#endif /* LWPKT_CFG_USE_FLAGS */
#if LWPKT_CFG_USE_CMD
        printf("Packet cmd: 0x%02X\r\n", (unsigned)lwpkt_get_cmd(&pkt));
#endif /* LWPKT_CFG_USE_CMD */
        printf("Packet data length: 0x%08X\r\n", (unsigned)lwpkt_get_data_len(&pkt));
        if ((len = lwpkt_get_data_len(&pkt)) > 0) {
            uint8_t* d = lwpkt_get_data(&pkt);
            printf("Packet data: ");
            for (size_t i = 0; i < len; ++i) {
                printf("0x%02X ", (unsigned)d[i]);
            }
            printf("\r\n");
        }

        /* Check who should be dedicated receiver */
#if LWPKT_CFG_USE_ADDR
        if (lwpkt_is_for_me(&pkt)) {
            printf("Packet is for me\r\n");
        } else if (lwpkt_is_broadcast(&pkt)) {
            printf("Packet is broadcast to all devices\r\n");
        } else {
            printf("Packet is for device ID: 0x%08X\r\n", (unsigned)lwpkt_get_to_addr(&pkt));
        }
#endif /* LWPKT_CFG_USE_ADDR */
    } else if (res == lwpktINPROG) {
        printf("Packet is still in progress, did not receive yet all bytes..\r\n");
    } else {
        printf("Packet is not valid!\r\n");
    }
}
