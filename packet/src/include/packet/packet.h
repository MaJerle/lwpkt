/**
 * \file            packet.c
 * \brief           Packet protocol manager
 */

/*
 * Copyright (c) 2020 Tilen MAJERLE
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file is part of packet protocol library.
 *
 * Author:          Tilen MAJERLE <tilen@majerle.eu>
 * Version:         $_version_$
 */
#ifndef PACKET_PROTOCOL_HDR_H
#define PACKET_PROTOCOL_HDR_H

#include <string.h>
#include <stdint.h>
#include "ringbuff/ringbuff.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \defgroup        PACKET Packet protocol
 * \brief           Packet protocol manager
 * \{
 */

#ifndef PKT_MAX_DATA_LEN
#define PKT_MAX_DATA_LEN                256
#endif

typedef enum {
    PKT_STATE_START = 0x00,
    PKT_STATE_FROM,
    PKT_STATE_TO,
    PKT_STATE_CMD,
    PKT_STATE_LEN,
    PKT_STATE_DATA,
    PKT_STATE_CRC,
    PKT_STATE_STOP,
} pkt_state_t;

typedef enum {
    pktOK = 0x00,
    pktERR,
} pktr_t;

typedef struct {
    uint8_t addr;
    uint8_t data[PKT_MAX_DATA_LEN];

    struct {
        pkt_state_t state;
        uint8_t from;
        uint8_t to;
        uint8_t cmd;
        size_t len;
        size_t index;
    } m;
} pkt_t;

pktr_t  packet_init(pkt_t* pkt, uint8_t addr);
pktr_t  packet_set_addr(pkt_t* pkt, uint8_t addr);
pktr_t  packet_process(pkt_t* pkt, RINGBUFF_VOLATILE ringbuff_t* rx_rb);
pktr_t  packet_write(pkt_t* pkt, RINGBUFF_VOLATILE ringbuff_t* tx_rb, uint8_t to, uint8_t cmd, const void* data, size_t len);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PACKET_PROTOCOL_HDR_H */
