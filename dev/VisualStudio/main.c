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
    
    /* Initialize packet and its buffers with default address */
    pkt_init(&pkt, 0x12);
    ringbuff_init(&pkt_tx_rb, pkt_tx_rb_data, sizeof(pkt_tx_rb_data));
    ringbuff_init(&pkt_rx_rb, pkt_rx_rb_data, sizeof(pkt_rx_rb_data));

    res = pkt_write(&pkt, &pkt_tx_rb, 0x11, 0x85, data, strlen(data));
    res = pkt_read(&pkt, &pkt_tx_rb);
    return 0;
}
