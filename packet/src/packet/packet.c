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
#include <string.h>
#include "packet/packet.h"

#define PKT_IS_VALID(p)             ((p) != NULL)
#define PKT_SET_STATE(p, s)         do { (p)->m.state = (s); (p)->m.index = 0; } while (0)
#define PKT_RESET(p)                do { memset((p), 0x00, sizeof((p)->m)); (p)->m.state = PKT_STATE_START; } while (0)

/* Start and STOP bytes definition */
#define PKT_START_BYTE              0xAA
#define PKT_STOP_BYTE               0x55

/**
 * \brief           Add new value to CRC instance
 * \param[in]       crc: CRC instance
 * \param[in]       in: Input data in byte format
 * \param[in]       len: Number of bytes to process
 * \return          Current CRC calculated value after all bytes or `0` on error input data
 */
static uint8_t
crc_in(pkt_crc_t* c, const void* in, const size_t len) {
    const uint8_t* d = in;

    if (c == NULL || in == NULL || len == 0) {
        return 0;
    }

    for (size_t i = 0; i < len; ++i, ++d) {
        uint8_t inbyte = *d;
        for (uint8_t j = 8; j > 0; --j) {
            uint8_t mix = (c->crc ^ inbyte) & 0x01;
            c->crc >>= 1;
            if (mix > 0) {
                c->crc ^= 0x8C;
            }
            inbyte >>= 0x01;
        }
    }
    return c->crc;
}

/**
 * \brief           Initialize CRC instance to default values
 * \param[in]       crc: CRC instance
 */
static void
crc_init(pkt_crc_t* c) {
    memset(c, 0x00, sizeof(*c));
}

pktr_t
pkt_init(pkt_t* pkt, uint8_t addr) {
    if (pkt == NULL) {
        return pktERR;
    }
    
    memset(pkt, 0x00, sizeof(*pkt));
    pkt->addr = addr;

    PKT_RESET(pkt);

    return pktOK;
}

pktr_t
pkt_set_addr(pkt_t* pkt, uint8_t addr) {
    if (!PKT_IS_VALID(pkt)) {
        return pktERR;
    }

    return pktOK;
}

/**
 * \brief           Read raw data from RX buffer and prepare packet
 * \param[in]       pkt: Packet instance
 * \param[in]       rx_rb: RX ringbuffer to read received data from
 * \return          \ref pktVALID when packet valid, member of \ref pktr_t otherwise
 */
pktr_t
pkt_read(pkt_t* pkt, RINGBUFF_VOLATILE ringbuff_t* rx_rb) {
    uint8_t b;
    if (!PKT_IS_VALID(pkt)) {
        return pktERR;
    }

    /* Process bytes from ringbuffer */
    while (ringbuff_read(rx_rb, &b, 1) == 1) {
        switch (pkt->m.state) {
            case PKT_STATE_START: {
                if (b == PKT_START_BYTE) {
                    crc_init(&pkt->m.crc);
                    PKT_SET_STATE(pkt, PKT_STATE_FROM);
                }
                break;
            }
            case PKT_STATE_FROM: {
                pkt->m.from = b;
                crc_in(&pkt->m.crc, &b, 1);
                PKT_SET_STATE(pkt, PKT_STATE_TO);
                break;
            }
            case PKT_STATE_TO: {
                pkt->m.to = b;
                crc_in(&pkt->m.crc, &b, 1);
                PKT_SET_STATE(pkt, PKT_STATE_CMD);
                break;
            }
            case PKT_STATE_CMD: {
                pkt->m.cmd = b;
                crc_in(&pkt->m.crc, &b, 1);
                PKT_SET_STATE(pkt, PKT_STATE_LEN);
                break;
            }
            case PKT_STATE_LEN: {
                pkt->m.len |= (b & 0x7F) << ((size_t)7 * (size_t)pkt->m.index);
                ++pkt->m.index;
                crc_in(&pkt->m.crc, &b, 1);
                
                /* Last length bytes has MSB bit set to 0 */
                if (!(b & 0x80)) {
                    if (pkt->m.len == 0) {
                        PKT_SET_STATE(pkt, PKT_STATE_CRC);
                    } else {
                        PKT_SET_STATE(pkt, PKT_STATE_DATA);
                    }
                }
                break;
            }
            case PKT_STATE_DATA: {
                pkt->data[pkt->m.index] = b;
                ++pkt->m.index;
                crc_in(&pkt->m.crc, &b, 1);
                if (pkt->m.index == pkt->m.len) {
                    PKT_SET_STATE(pkt, PKT_STATE_CRC);
                }
                break;
            }
            case PKT_STATE_CRC: {
                crc_in(&pkt->m.crc, &b, 1);
                if (pkt->m.crc.crc == 0x00) {
                    PKT_SET_STATE(pkt, PKT_STATE_STOP);
                } else {
                    PKT_RESET(pkt);
                    return pktERRCRC;
                }
                PKT_SET_STATE(pkt, PKT_STATE_STOP);
                break;
            }
            case PKT_STATE_STOP: {
                PKT_RESET(pkt);             /* Reset packet state */
                if (b == PKT_STOP_BYTE) {
                    return pktVALID;        /* Packet fully valid, take data from it */
                } else {
                    return pktERRSTOP;      /* Packet is missin STOP byte! */
                }
            }
        }
    }
    if (pkt->m.state == PKT_STATE_START) {
        return pktWAITDATA;
    }
    return pktINPROG;
}

/**
 * \brief           Write packet data to TX ringbuffer
 * \param[in]       pkt: Packet instance
 * \param[in]       tx_rb: TX ringbuffer to write data to be transmitted afterwards
 * \param[in]       to_addr: End device address
 * \param[in]       cmd: Packet command
 * \param[in]       data: Pointer to input data. Set to `NULL` if not used
 * \param[in]       len: Length of input data. Must be set to `0` if `data == NULL`
 * \return          \ref pktOK on success, member of \ref pktr_t otherwise
 */
pktr_t
pkt_write(pkt_t* pkt, RINGBUFF_VOLATILE ringbuff_t* tx_rb, uint8_t to_addr,
            uint8_t cmd, const void* data, size_t len) {
    pkt_crc_t crc;
    size_t org_len = len;
    uint8_t b;

    if (!PKT_IS_VALID(pkt)) {
        return pktERR;
    }

    crc_init(&crc);                             /* Reset CRC instance */

    /* Start byte */
    b = PKT_START_BYTE;
    ringbuff_write(tx_rb, &b, 1);

    /* From address */
    ringbuff_write(tx_rb, &pkt->addr, 1);
    crc_in(&crc, &pkt->addr, 1);

    /* To address */
    b = to_addr;
    ringbuff_write(tx_rb, &b, 1);
    crc_in(&crc, &b, 1);

    /* CMD byte */
    b = cmd;
    ringbuff_write(tx_rb, &b, 1);
    crc_in(&crc, &b, 1);

    /* Length bytes */
    do {
        b = (len & 0x7F) | (len > 0x7F ? 0x80 : 0x00);
        ringbuff_write(tx_rb, &b, 1);
        crc_in(&crc, &b, 1);
        len >>= 7;
    } while (len > 0);

    /* Data bytes */
    if (org_len > 0) {
        ringbuff_write(tx_rb, data, org_len);
        crc_in(&crc, data, org_len);
    }

    /* CRC byte */
    ringbuff_write(tx_rb, &crc.crc, 1);

    /* Stop byte */
    b = PKT_STOP_BYTE;
    ringbuff_write(tx_rb, &b, 1);

    return pktOK;
}

/**
 * \brief           Reset packet state
 * \param[in]       pkt: Packet instance
 */
pktr_t
pkt_reset(pkt_t* pkt) {
    if (!PKT_IS_VALID(pkt)) {
        return pktERR;
    }
    PKT_RESET(pkt);
    return pktOK;
}
