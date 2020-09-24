/**
 * \file            lwpkt.c
 * \brief           Lightweight packet protocol
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
#include "lwpkt/lwpkt.h"

#define LWPKT_IS_VALID(p)           ((p) != NULL)
#define LWPKT_SET_STATE(p, s)       do { (p)->m.state = (s); (p)->m.index = 0; } while (0)
#define LWPKT_RESET(p)              do { memset(&(p)->m, 0x00, sizeof((p)->m)); (p)->m.state = LWPKT_STATE_START; } while (0)

/* Start and STOP bytes definition */
#define LWPKT_START_BYTE            0xAA
#define LWPKT_STOP_BYTE             0x55

/**
 * \brief           Add new value to CRC instance
 * \param[in]       crc: CRC instance
 * \param[in]       in: Input data in byte format
 * \param[in]       len: Number of bytes to process
 * \return          Current CRC calculated value after all bytes or `0` on error input data
 */
static uint8_t
prv_crc_in(lwpkt_crc_t* c, const void* in, const size_t len) {
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
prv_crc_init(lwpkt_crc_t* c) {
    memset(c, 0x00, sizeof(*c));
}

/**
 * \brief           Initialize packet instance and set device address
 * \param[in]       pkt: Packet instance
 * \param[in]       addr: Current device address
 * \return          \ref lwpktOK on success, member of \ref lwpktr_t otherwise
 */
lwpktr_t
lwpkt_init(lwpkt_t* pkt, uint8_t addr) {
    if (pkt == NULL) {
        return lwpktERR;
    }

    memset(pkt, 0x00, sizeof(*pkt));
    pkt->addr = addr;

    LWPKT_RESET(pkt);

    return lwpktOK;
}

/**
 * \brief           Set device address for packet instance
 * \param[in]       pkt: Packet instance
 * \param[in]       addr: New device address
 * \return          \ref lwpktOK on success, member of \ref lwpktr_t otherwise
 */
lwpktr_t
lwpkt_set_addr(lwpkt_t* pkt, uint8_t addr) {
    if (!LWPKT_IS_VALID(pkt)) {
        return lwpktERR;
    }

    pkt->addr = addr;

    return lwpktOK;
}

/**
 * \brief           Read raw data from RX buffer and prepare packet
 * \param[in]       pkt: Packet instance
 * \param[in]       rx_rb: RX ringbuffer to read received data from
 * \return          \ref pktVALID when packet valid, member of \ref lwpktr_t otherwise
 */
lwpktr_t
lwpkt_read(lwpkt_t* pkt, LWRB_VOLATILE lwrb_t* rx_rb) {
    uint8_t b;
    if (!LWPKT_IS_VALID(pkt)) {
        return lwpktERR;
    }

    /* Process bytes from RX ringbuffer */
    /* Read byte by byte and go through state machine */
    while (lwrb_read(rx_rb, &b, 1) == 1) {
        switch (pkt->m.state) {
            case LWPKT_STATE_START: {
                if (b == LWPKT_START_BYTE) {
                    prv_crc_init(&pkt->m.crc);
                    LWPKT_SET_STATE(pkt, LWPKT_STATE_FROM);
                }
                break;
            }
            case LWPKT_STATE_FROM: {
                pkt->m.from = b;
                prv_crc_in(&pkt->m.crc, &b, 1);
                LWPKT_SET_STATE(pkt, LWPKT_STATE_TO);
                break;
            }
            case LWPKT_STATE_TO: {
                pkt->m.to = b;
                prv_crc_in(&pkt->m.crc, &b, 1);
                LWPKT_SET_STATE(pkt, LWPKT_STATE_CMD);
                break;
            }
            case LWPKT_STATE_CMD: {
                pkt->m.cmd = b;
                prv_crc_in(&pkt->m.crc, &b, 1);
                LWPKT_SET_STATE(pkt, LWPKT_STATE_LEN);
                break;
            }
            case LWPKT_STATE_LEN: {
                pkt->m.len |= (b & 0x7F) << ((size_t)7 * (size_t)pkt->m.index);
                ++pkt->m.index;
                prv_crc_in(&pkt->m.crc, &b, 1);

                /* Last length bytes has MSB bit set to 0 */
                if ((b & 0x80) == 0x00) {
                    if (pkt->m.len == 0) {
                        LWPKT_SET_STATE(pkt, LWPKT_STATE_CRC);
                    } else {
                        LWPKT_SET_STATE(pkt, LWPKT_STATE_DATA);
                    }
                }
                break;
            }
            case LWPKT_STATE_DATA: {
                pkt->data[pkt->m.index] = b;
                ++pkt->m.index;
                prv_crc_in(&pkt->m.crc, &b, 1);
                if (pkt->m.index == pkt->m.len) {
                    LWPKT_SET_STATE(pkt, LWPKT_STATE_CRC);
                }
                break;
            }
            case LWPKT_STATE_CRC: {
                prv_crc_in(&pkt->m.crc, &b, 1);
                if (pkt->m.crc.crc == 0x00) {
                    LWPKT_SET_STATE(pkt, LWPKT_STATE_STOP);
                } else {
                    LWPKT_RESET(pkt);
                    return lwpktERRCRC;
                }
                LWPKT_SET_STATE(pkt, LWPKT_STATE_STOP);
                break;
            }
            case LWPKT_STATE_STOP: {
                LWPKT_SET_STATE(pkt, LWPKT_STATE_START);/* Reset packet state */
                if (b == LWPKT_STOP_BYTE) {
                    return lwpktVALID;          /* Packet fully valid, take data from it */
                } else {
                    return lwpktERRSTOP;        /* Packet is missin STOP byte! */
                }
            }
        }
    }
    if (pkt->m.state == LWPKT_STATE_START) {
        return lwpktWAITDATA;
    }
    return lwpktINPROG;
}

/**
 * \brief           Write packet data to TX ringbuffer
 * \param[in]       pkt: Packet instance
 * \param[in]       tx_rb: TX ringbuffer to write data to be transmitted afterwards
 * \param[in]       to_addr: End device address
 * \param[in]       cmd: Packet command
 * \param[in]       data: Pointer to input data. Set to `NULL` if not used
 * \param[in]       len: Length of input data. Must be set to `0` if `data == NULL`
 * \return          \ref lwpktOK on success, member of \ref lwpktr_t otherwise
 */
lwpktr_t
lwpkt_write(lwpkt_t* pkt, LWRB_VOLATILE lwrb_t* tx_rb, uint8_t to_addr,
          uint8_t cmd, const void* data, size_t len) {
    lwpkt_crc_t crc;
    size_t org_len = len;
    uint8_t b;

    if (!LWPKT_IS_VALID(pkt)) {
        return lwpktERR;
    }

    prv_crc_init(&crc);

    /* Start byte */
    b = LWPKT_START_BYTE;
    lwrb_write(tx_rb, &b, 1);

    /* From address */
    lwrb_write(tx_rb, &pkt->addr, 1);
    prv_crc_in(&crc, &pkt->addr, 1);

    /* To address */
    b = to_addr;
    lwrb_write(tx_rb, &b, 1);
    prv_crc_in(&crc, &b, 1);

    /* CMD byte */
    b = cmd;
    lwrb_write(tx_rb, &b, 1);
    prv_crc_in(&crc, &b, 1);

    /* Length bytes */
    do {
        b = (len & 0x7F) | (len > 0x7F ? 0x80 : 0x00);
        lwrb_write(tx_rb, &b, 1);
        prv_crc_in(&crc, &b, 1);
        len >>= 7;
    } while (len > 0);

    /* Data bytes */
    if (org_len > 0) {
        lwrb_write(tx_rb, data, org_len);
        prv_crc_in(&crc, data, org_len);
    }

    /* CRC byte */
    lwrb_write(tx_rb, &crc.crc, 1);

    /* Stop byte */
    b = LWPKT_STOP_BYTE;
    lwrb_write(tx_rb, &b, 1);

    return lwpktOK;
}

/**
 * \brief           Reset packet state
 * \param[in]       pkt: Packet instance
 */
lwpktr_t
lwpkt_reset(lwpkt_t* pkt) {
    if (!LWPKT_IS_VALID(pkt)) {
        return lwpktERR;
    }
    LWPKT_RESET(pkt);
    return lwpktOK;
}
