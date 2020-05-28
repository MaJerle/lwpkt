// main.c : Defines the entry point for the console application.
//

#include <stdio.h>
#include <string.h>
#include "packet/packet.h"

pkt_t pkt;
ringbuff_t pkt_tx_rb, pkt_rx_rb;
uint8_t pkt_tx_rb_data[64], pkt_rx_rb_data[64];

const char* data = "Hello World\r\n";

int
main() {
    pktr_t res;
    uint8_t b;
    
    /* Initialize packet and its buffers with default address */
    pkt_init(&pkt, 0x12);
    ringbuff_init(&pkt_tx_rb, pkt_tx_rb_data, sizeof(pkt_tx_rb_data));
    ringbuff_init(&pkt_rx_rb, pkt_rx_rb_data, sizeof(pkt_rx_rb_data));

    /* Write packet data to TX ringbuff... */
    res = pkt_write(&pkt, &pkt_tx_rb, 0x11, 0x85, data, strlen(data));

    /* Transmit TX buffer data to communication line... */
    /* Write received data from communication line to RX buffer */

    /* Copy buffers */
    while (ringbuff_read(&pkt_tx_rb, &b, 1) == 1) {
        ringbuff_write(&pkt_rx_rb, &b, 1);
    }

    /* Read data from RX ringbuffer */
    res = pkt_read(&pkt, &pkt_rx_rb);

    if (res == pktVALID) {
        size_t len;

        /* Packet is valid */
        printf("Packet is valid!\r\n");

        /* Print debug messages for packet */
        printf("Packet from: 0x%02X\r\n", (unsigned)pkt_get_from_addr(&pkt));
        printf("Packet to: 0x%02X\r\n", (unsigned)pkt_get_to_addr(&pkt));
        printf("Packet cmd: 0x%02X\r\n", (unsigned)pkt_get_cmd(&pkt));
        printf("Packet data length: 0x%02X\r\n", (unsigned)pkt_get_data_len(&pkt));
        if ((len = pkt_get_data_len(&pkt)) > 0) {
            uint8_t* d = pkt_get_data(&pkt);
            printf("Packet data: ");
            for (size_t i = 0; i < len; ++i) {
                printf("0x%02X ", (unsigned)d[i]);
            }
            printf("\r\n");
        }

        /* Check who should be dedicated receiver */
        if (pkt_is_for_me(&pkt)) {
            printf("Packet is for me\r\n");
        } else if (pkt_is_broadcast(&pkt)) {
            printf("Packet is broadcast to all devices\r\n");
        } else {
            printf("Packet is for device ID: 0x%02X\r\n", (unsigned)pkt_get_to_addr(&pkt));
        }
    } else {
        printf("Packet is not valid!\r\n");
    }

    return 0;
}
