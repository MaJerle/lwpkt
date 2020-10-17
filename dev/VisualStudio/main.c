// main.c : Defines the entry point for the console application.
//

#include <stdio.h>
#include <string.h>
#include "lwpkt/lwpkt.h"

lwpkt_t pkt;
lwrb_t pkt_tx_rb, pkt_rx_rb;
uint8_t pkt_tx_rb_data[64], pkt_rx_rb_data[64];

const char* data = "Hello World\r\n";

int
main() {
    lwpktr_t res;
    uint8_t b;

    /* Initialize buffers for packet protocol, for TX and RX */
    lwrb_init(&pkt_tx_rb, pkt_tx_rb_data, sizeof(pkt_tx_rb_data));
    lwrb_init(&pkt_rx_rb, pkt_rx_rb_data, sizeof(pkt_rx_rb_data));

    /* Initialize packet and its buffers with default address */
    lwpkt_init(&pkt, &pkt_tx_rb, &pkt_rx_rb);
#if LWPKT_CFG_USE_ADDR
    lwpkt_set_addr(&pkt, 0x12);
#endif /* LWPKT_CFG_USE_ADDR */

    /* Write packet data to TX ringbuff... */
    res = lwpkt_write(&pkt,
#if LWPKT_CFG_USE_ADDR
        0x11,
#endif /* LWPKT_CFG_USE_ADDR */
#if LWPKT_CFG_USE_CMD
        0x85,
#endif /* LWPKT_CFG_USE_CMD */
        data, strlen(data));

    /* Transmit TX buffer data to communication line... */
    /* Write received data from communication line to RX buffer */

    /* Copy buffers */
    while (lwrb_read(&pkt_tx_rb, &b, 1) == 1) {
        lwrb_write(&pkt_rx_rb, &b, 1);
    }

    /* Read data from RX ringbuffer */
    res = lwpkt_read(&pkt);

    if (res == lwpktVALID) {
        size_t len;

        /* Packet is valid */
        printf("Packet is valid!\r\n");

        /* Print debug messages for packet */
#if LWPKT_CFG_USE_ADDR
        printf("Packet from: 0x%02X\r\n", (unsigned)lwpkt_get_from_addr(&pkt));
        printf("Packet to: 0x%02X\r\n", (unsigned)lwpkt_get_to_addr(&pkt));
#endif /* LWPKT_CFG_USE_ADDR */
#if LWPKT_CFG_USE_CMD
        printf("Packet cmd: 0x%02X\r\n", (unsigned)lwpkt_get_cmd(&pkt));
#endif /* LWPKT_CFG_USE_CMD */
        printf("Packet data length: 0x%02X\r\n", (unsigned)lwpkt_get_data_len(&pkt));
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
            printf("Packet is for device ID: 0x%02X\r\n", (unsigned)lwpkt_get_to_addr(&pkt));
        }
#endif /* LWPKT_CFG_USE_ADDR */
    } else {
        printf("Packet is not valid!\r\n");
    }

    return 0;
}
