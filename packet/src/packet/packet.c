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
#include "packet/packet.h"

#define PKT_IS_VALID(p)     ((p) != NULL)

#define PKT_START_BYTE      0xAA
#define PKT_STOP_BYTE       0x55

pktr_t
pkt_init(pkt_t* pkt, uint8_t addr) {
    if (pkt == NULL) {
        return pktERR;
    }

    return pktOK;
}

pktr_t
pkt_set_addr(pkt_t* pkt, uint8_t addr) {
    if (!PKT_IS_VALID(pkt)) {
        return pktERR;
    }

    return pktOK;
}

pktr_t
pkt_process(pkt_t* pkt, RINGBUFF_VOLATILE ringbuff_t* rx_rb) {
    uint8_t b;
    if (!PKT_IS_VALID(pkt)) {
        return pktERR;
    }

    /* Process bytes from ringbuffer */
    while (ringbuff_read(rx_rb, &b, 1) == 1) {
        switch (pkt->m.state) {
            case PKT_STATE_START: {
                if (b == PKT_START_BYTE) {
                    pkt->m.state = PKT_STATE_FROM;
                }
                break;
            }
            case PKT_STATE_FROM: {
                pkt->m.from = b;
                pkt->m.state = PKT_STATE_TO;
                break;
            }
            case PKT_STATE_TO: {
                pkt->m.to = b;
                pkt->m.state = PKT_STATE_CMD;
                break;
            }
            case PKT_STATE_CMD: {
                pkt->m.cmd = b;
                pkt->m.state = PKT_STATE_LEN;
                break;
            }
            case PKT_STATE_LEN: {
                pkt->m.len = (pkt->m.len << 7) | (b & 0x7F);
                
                /* Last length bytes has MSB bit set to 0 */
                if (!(b & 0x7F)) {
                    if (pkt->m.len == 0) {
                        pkt->m.state = PKT_STATE_CRC;
                    } else {
                        pkt->m.state = PKT_STATE_DATA;
                    }
                }
                break;
            }
            case PKT_STATE_DATA: {
                pkt->data[pkt->m.index] = b;
                pkt->m.index++;
                if (pkt->m.index == pkt->m.len) {
                    pkt->m.state = PKT_STATE_CRC;
                }
                break;
            }
            case PKT_STATE_CRC: {
                break;
            }
            case PKT_STATE_STOP: {
                if (b == PKT_STOP_BYTE) {
                    /* Packet valid! */
                } else {
                    /* Packet invalid! */
                }
            }
        }
    }

    return pktOK;
}

pktr_t
pkt_write(pkt_t* pkt, RINGBUFF_VOLATILE ringbuff_t* tx_rb, uint8_t to,
            uint8_t cmd, const void* data, size_t len) {
    if (!PKT_IS_VALID(pkt)) {
        return pktERR;
    }

    return pktOK;
}
