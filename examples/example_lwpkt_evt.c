#include <stdio.h>
#include "lwpkt/lwpkt.h"

/* LwPKT data */
static lwpkt_t pkt;
static lwrb_t pkt_tx_rb, pkt_rx_rb;
static uint8_t pkt_tx_rb_data[64], pkt_rx_rb_data[64];

/* Data to read and write */
static const char* data = "Hello World\r\n";

/**
 * \brief           LwPKT application callback
 */
static void
my_lwpkt_evt_fn(lwpkt_t* pkt, lwpkt_evt_type_t type) {
    switch (type) {
        case LWPKT_EVT_PKT: {
            printf("Valid packet received..\r\n");

            /* Packet is valid */
            printf("Packet is valid!\r\n");

            /* Print debug messages for packet */
#if LWPKT_CFG_USE_ADDR
            printf("Packet from: 0x%08X\r\n", (unsigned)lwpkt_get_from_addr(pkt));
            printf("Packet to: 0x%08X\r\n", (unsigned)lwpkt_get_to_addr(pkt));
#endif /* LWPKT_CFG_USE_ADDR */
#if LWPKT_CFG_USE_CMD
            printf("Packet cmd: 0x%08X\r\n", (unsigned)lwpkt_get_cmd(pkt));
#endif /* LWPKT_CFG_USE_CMD */
            printf("Packet data length: 0x%08X\r\n", (unsigned)lwpkt_get_data_len(pkt));

            /* Do other thins... */
            break;
        }
        case LWPKT_EVT_TIMEOUT: {
            printf("Timeout detected during read operation..\r\n");
            break;
        }
        default: {
            break;
        }
    }
}

/**
 * \brief           LwPKT example code with event feature
 */
void
example_lwpkt_evt(void) {
    lwpktr_t res;
    uint32_t time;
    uint8_t b;

    printf("---\r\nLwPKT event type..\r\n\r\n");

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
    while (lwrb_read(&pkt_tx_rb, &b, 1) == 1) {
        lwrb_write(&pkt_rx_rb, &b, 1);
    }

    /*
     * Here we have our data in RX buffer
     * means we received data over network interface
     */

    /* Set callback function */
    lwpkt_set_evt_fn(&pkt, my_lwpkt_evt_fn);

    /* Now call process function instead */
    time = 100; /* Get_current_time_in_milliseconds */
    lwpkt_process(&pkt, time);

    (void)res;
}