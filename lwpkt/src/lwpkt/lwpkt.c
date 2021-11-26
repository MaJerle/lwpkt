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
 * This file is part of LwPKT - Lightweight packet protocol library.
 *
 * Author:          Tilen MAJERLE <tilen@majerle.eu>
 * Version:         v1.2.0
 */
#include <string.h>
#include "lwpkt/lwpkt.h"

#define LWPKT_IS_VALID(p)           ((p) != NULL)
#define LWPKT_SET_STATE(p, s)       do { (p)->m.state = (s); (p)->m.index = 0; } while (0)
#define LWPKT_RESET(p)              do { memset(&(p)->m, 0x00, sizeof((p)->m)); (p)->m.state = LWPKT_STATE_START; } while (0)

/* Start and STOP bytes definition */
#define LWPKT_START_BYTE            0xAA
#define LWPKT_STOP_BYTE             0x55

#if LWPKT_CFG_USE_CRC
#define WRITE_WITH_CRC(crc, tx_rb, b, len) do { \
    lwrb_write((tx_rb), (b), (len));            \
    prv_crc_in((crc), (b), (len));              \
} while (0)
#define ADD_IN_TO_CRC(crc, val, len)    prv_crc_in((crc), (val), (len))
#define INIT_CRC(crc)                   prv_crc_init((crc))
#else /* LWPKT_CFG_USE_CRC */
#define WRITE_WITH_CRC(crc, tx_rb, b, len) do { \
    lwrb_write((tx_rb), (b), (len));            \
} while (0)
#define ADD_IN_TO_CRC(val)
#define INIT_CRC(val)
#endif /* !LWPKT_CFG_USE_CRC */

#if LWPKT_CFG_USE_EVT
#define SEND_EVT(p, t)              if ((p)->evt_fn != NULL) { (p)->evt_fn((p), (t)); }
#else /* LWPKT_CFG_USE_EVT */
#define SEND_EVT(p, t)              do {(void)(p); (void)(t); } while (0)
#endif /* !LWPKT_CFG_USE_EVT */

#if LWPKT_CFG_USE_CRC || __DOXYGEN__

/**
 * \brief           Add new value to CRC instance
 * \param[in]       c: CRC instance
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
 * \param[in]       c: CRC instance
 */
static void
prv_crc_init(lwpkt_crc_t* c) {
    memset(c, 0x00, sizeof(*c));
}

#endif /* LWPKT_CFG_USE_CRC || __DOXYGEN__ */

/**
 * \brief           Initialize packet instance and set device address
 * \param[in]       pkt: Packet instance
 * \param[in]       tx_rb: TX LwRB instance for data write
 * \param[in]       rx_rb: RX LwRB instance for data read
 * \return          \ref lwpktOK on success, member of \ref lwpktr_t otherwise
 */
lwpktr_t
lwpkt_init(lwpkt_t* pkt, LWRB_VOLATILE lwrb_t* tx_rb, LWRB_VOLATILE lwrb_t* rx_rb) {
    if (pkt == NULL) {
        return lwpktERR;
    }

    memset(pkt, 0x00, sizeof(*pkt));
    LWPKT_RESET(pkt);

    pkt->tx_rb = tx_rb;
    pkt->rx_rb = rx_rb;

    return lwpktOK;
}

#if LWPKT_CFG_USE_ADDR || __DOXYGEN__

/**
 * \brief           Set device address for packet instance
 * \param[in]       pkt: Packet instance
 * \param[in]       addr: New device address
 * \return          \ref lwpktOK on success, member of \ref lwpktr_t otherwise
 */
lwpktr_t
lwpkt_set_addr(lwpkt_t* pkt, lwpkt_addr_t addr) {
    if (!LWPKT_IS_VALID(pkt)) {
        return lwpktERR;
    }
    pkt->addr = addr;

    return lwpktOK;
}

#endif /* LWPKT_CFG_USE_ADDR || __DOXYGEN__ */

/**
 * \brief           Read raw data from RX buffer and prepare packet
 * \param[in]       pkt: Packet instance
 * \return          \ref lwpktVALID when packet valid, member of \ref lwpktr_t otherwise
 */
lwpktr_t
lwpkt_read(lwpkt_t* pkt) {
    lwpktr_t res = lwpktOK;
    uint8_t b, e = 0;

    if (!LWPKT_IS_VALID(pkt)) {
        return lwpktERR;
    }

    /* Process bytes from RX ringbuffer */
    /* Read byte by byte and go through state machine */
    while (lwrb_read(pkt->rx_rb, &b, 1) == 1) {
        e = 1;
        switch (pkt->m.state) {
            case LWPKT_STATE_START: {
                if (b == LWPKT_START_BYTE) {
                    LWPKT_RESET(pkt);           /* Reset instance and make it ready for receiving */
                    INIT_CRC(&pkt->m.crc);

                    LWPKT_SET_STATE(pkt, LWPKT_CFG_USE_ADDR ? LWPKT_STATE_FROM : (LWPKT_CFG_USE_CMD ? LWPKT_STATE_CMD : LWPKT_STATE_LEN));
                }
                break;
            }
#if LWPKT_CFG_USE_ADDR
            case LWPKT_STATE_FROM: {
                ADD_IN_TO_CRC(&pkt->m.crc, &b, 1);
#if LWPKT_CFG_ADDR_EXTENDED
                pkt->m.from |= (b & 0x7F) << ((size_t)7 * (size_t)pkt->m.index++);
#else /* LWPKT_CFG_ADDR_EXTENDED */
                pkt->m.from = b;
#endif /* !LWPKT_CFG_ADDR_EXTENDED */

                /* Check if ready to move forward */
                if (!LWPKT_CFG_ADDR_EXTENDED    /* Default mode goes straight with single byte */
                    || (LWPKT_CFG_ADDR_EXTENDED && (b & 0x80) == 0x00)) {   /* Extended mode must have MSB set to 0 */
                    LWPKT_SET_STATE(pkt, LWPKT_STATE_TO);
                }
                break;
            }
            case LWPKT_STATE_TO: {
                ADD_IN_TO_CRC(&pkt->m.crc, &b, 1);
#if LWPKT_CFG_ADDR_EXTENDED
                pkt->m.to |= (b & 0x7F) << ((size_t)7 * (size_t)pkt->m.index++);
#else /* LWPKT_CFG_ADDR_EXTENDED */
                pkt->m.to = b;
#endif /* !LWPKT_CFG_ADDR_EXTENDED */

                /* Check if ready to move forward */
                if (!LWPKT_CFG_ADDR_EXTENDED    /* Default mode goes straight */
                    || (LWPKT_CFG_ADDR_EXTENDED && (b & 0x80) == 0x00)) {   /* Extended mode must have MSB set to 0 */
                    LWPKT_SET_STATE(pkt, LWPKT_CFG_USE_CMD ? LWPKT_STATE_CMD : LWPKT_STATE_LEN);
                }
                break;
            }
#endif /* LWPKT_CFG_USE_ADDR */
#if LWPKT_CFG_USE_CMD
            case LWPKT_STATE_CMD: {
                pkt->m.cmd = b;
                ADD_IN_TO_CRC(&pkt->m.crc, &b, 1);
                LWPKT_SET_STATE(pkt, LWPKT_STATE_LEN);
                break;
            }
#endif /* LWPKT_CFG_USE_CMD */
            case LWPKT_STATE_LEN: {
                pkt->m.len |= (b & 0x7F) << ((size_t)7 * (size_t)pkt->m.index++);
                ADD_IN_TO_CRC(&pkt->m.crc, &b, 1);

                /* Last length bytes has MSB bit set to 0 */
                if ((b & 0x80) == 0x00) {
                    if (pkt->m.len == 0) {
                        LWPKT_SET_STATE(pkt, LWPKT_CFG_USE_CRC ? LWPKT_STATE_CRC : LWPKT_STATE_STOP);
                    } else {
                        LWPKT_SET_STATE(pkt, LWPKT_STATE_DATA);
                    }
                }
                break;
            }
            case LWPKT_STATE_DATA: {
                pkt->data[pkt->m.index++] = b;
                ADD_IN_TO_CRC(&pkt->m.crc, &b, 1);
                if (pkt->m.index == pkt->m.len) {
                    LWPKT_SET_STATE(pkt, LWPKT_CFG_USE_CRC ? LWPKT_STATE_CRC : LWPKT_STATE_STOP);
                }
                break;
            }
#if LWPKT_CFG_USE_CRC
            case LWPKT_STATE_CRC: {
                ADD_IN_TO_CRC(&pkt->m.crc, &b, 1);
                if (pkt->m.crc.crc == 0x00) {
                    LWPKT_SET_STATE(pkt, LWPKT_STATE_STOP);
                } else {
                    LWPKT_RESET(pkt);
                    res = lwpktERRCRC;
                    goto retpre;
                }
                LWPKT_SET_STATE(pkt, LWPKT_STATE_STOP);
                break;
            }
#endif /* LWPKT_CFG_USE_CRC */
            case LWPKT_STATE_STOP: {
                LWPKT_SET_STATE(pkt, LWPKT_STATE_START);/* Reset packet state */
                if (b == LWPKT_STOP_BYTE) {
                    res = lwpktVALID;           /* Packet fully valid, take data from it */
                    goto retpre;
                } else {
                    res = lwpktERRSTOP;         /* Packet is missing STOP byte! */
                    goto retpre;
                }
            }
            default: {
                LWPKT_RESET(pkt);
                res = lwpktERR;                 /* Hard error */
                goto retpre;
            }
        }
    }
    if (e) {
        SEND_EVT(pkt, LWPKT_EVT_READ);          /* Send read event */
    }
    if (pkt->m.state == LWPKT_STATE_START) {
        return lwpktWAITDATA;
    }
    return lwpktINPROG;
retpre:
    SEND_EVT(pkt, LWPKT_EVT_READ);              /* Send read event */
    return res;
}

/**
 * \brief           Process packet instance and read new data
 * \param[in]       pkt: Packet instance
 * \param[in]       time: Current time in units of milliseconds
 * \return          \ref lwpktOK if processing OK, member of \ref lwpktr_t otherwise
 */
lwpktr_t
lwpkt_process(lwpkt_t* pkt, uint32_t time) {
    lwpktr_t pktres;

    if (pkt == NULL) {
        return lwpktERR;
    }

    /* Packet protocol data read */
    pktres = lwpkt_read(pkt);
    if (pktres == lwpktVALID) {
        pkt->last_rx_time = time;
        SEND_EVT(pkt, LWPKT_EVT_PKT);
    } else if (pktres == lwpktINPROG) {
        if ((time - pkt->last_rx_time) >= LWPKT_CFG_PROCESS_INPROG_TIMEOUT) {
            lwpkt_reset(pkt);
            pkt->last_rx_time = time;
            SEND_EVT(pkt, LWPKT_EVT_TIMEOUT);
        }
    } else {
        pkt->last_rx_time = time;
    }
    return lwpktOK;
}

/**
 * \brief           Write packet data to TX ringbuffer
 * \param[in]       pkt: Packet instance
 * \param[in]       to: End device address
 * \param[in]       cmd: Packet command
 * \param[in]       data: Pointer to input data. Set to `NULL` if not used
 * \param[in]       len: Length of input data. Must be set to `0` if `data == NULL`
 * \return          \ref lwpktOK on success, member of \ref lwpktr_t otherwise
 */
lwpktr_t
lwpkt_write(lwpkt_t* pkt,
#if LWPKT_CFG_USE_ADDR || __DOXYGEN__
    lwpkt_addr_t to,
#endif /* LWPKT_CFG_USE_ADDR || __DOXYGEN__ */
#if LWPKT_CFG_USE_CMD || __DOXYGEN__
    uint8_t cmd,
#endif /* LWPKT_CFG_USE_CMD || __DOXYGEN__ */
    const void* data, size_t len) {
#if LWPKT_CFG_USE_CRC
    lwpkt_crc_t crc;
#endif /* LWPKT_CFG_USE_CRC */
#if LWPKT_CFG_ADDR_EXTENDED
    lwpkt_addr_t addr;
#endif /* LWPKT_CFG_ADDR_EXTENDED */
    size_t org_len = len;
    uint8_t b;

    if (!LWPKT_IS_VALID(pkt)) {
        return lwpktERR;
    } else {
        /* Check for required memory for packet */
        size_t min_mem = 2, tmp_len = 0;
#if LWPKT_CFG_USE_ADDR && LWPKT_CFG_ADDR_EXTENDED
        lwpkt_addr_t tmp_addr;
#endif /* LWPKT_CFG_USE_ADDR && LWPKT_CFG_ADDR_EXTENDED */

        /* Addresses */
#if LWPKT_CFG_USE_ADDR
#if LWPKT_CFG_ADDR_EXTENDED
        tmp_addr = pkt->addr;
        do {
            ++min_mem;
            tmp_addr >>= 7;
        } while (tmp_addr > 0);
        tmp_addr = to;
        do {
            ++min_mem;
            tmp_addr >>= 7;
        } while (tmp_addr > 0);
#else /* LWPKT_CFG_ADDR_EXTENDED */
        min_mem += 2;
#endif /* !LWPKT_CFG_ADDR_EXTENDED */
#endif /* LWPKT_CFG_USE_ADDR */

#if LWPKT_CFG_USE_CMD
        /* CMD part */
        ++min_mem;
#endif /* LWPKT_CFG_USE_CMD */

        /* Data length */
        tmp_len = len;
        do {
            ++min_mem;
            tmp_len >>= 7;
        } while (tmp_len > 0);

        /* Data length */
        min_mem += len;

#if LWPKT_CFG_USE_CRC
        /* CRC part */
        ++min_mem;
#endif /* LWPKT_CFG_USE_CRC */

        /* Verify enough memory */
        if (lwrb_get_free(pkt->tx_rb) < min_mem) {
            return lwpktERRMEM;
        }
    }

#if LWPKT_CFG_USE_CRC
    prv_crc_init(&crc);
#endif /* LWPKT_CFG_USE_CRC */

    /* Start byte */
    b = LWPKT_START_BYTE;
    lwrb_write(pkt->tx_rb, &b, 1);

#if LWPKT_CFG_USE_ADDR
#if LWPKT_CFG_ADDR_EXTENDED
    /* FROM address */
    addr = pkt->addr;
    do {
        b = (addr & 0x7F) | (addr > 0x7F ? 0x80 : 0x00);
        WRITE_WITH_CRC(&crc, pkt->tx_rb, &b, 1);
        addr >>= 7;
    } while (addr > 0);

    /* TO address */
    addr = to;
    do {
        b = (addr & 0x7F) | (addr > 0x7F ? 0x80 : 0x00);
        WRITE_WITH_CRC(&crc, pkt->tx_rb, &b, 1);
        addr >>= 7;
    } while (addr > 0);
#else /* LWPKT_CFG_ADDR_EXTENDED */
    WRITE_WITH_CRC(&crc, pkt->tx_rb, &pkt->addr, 1);
    WRITE_WITH_CRC(&crc, pkt->tx_rb, &to, 1);
#endif /* !LWPKT_CFG_ADDR_EXTENDED */
#endif /* LWPKT_CFG_USE_ADDR */

#if LWPKT_CFG_USE_CMD
    /* CMD byte */
    WRITE_WITH_CRC(&crc, pkt->tx_rb, &cmd, 1);
#endif /* LWPKT_CFG_USE_CMD */

    /* Length bytes */
    do {
        b = (len & 0x7F) | (len > 0x7F ? 0x80 : 0x00);
        WRITE_WITH_CRC(&crc, pkt->tx_rb, &b, 1);
        len >>= 7;
    } while (len > 0);

    /* Data bytes */
    if (org_len > 0) {
        WRITE_WITH_CRC(&crc, pkt->tx_rb, data, org_len);
    }

#if LWPKT_CFG_USE_CRC
    /* CRC byte */
    lwrb_write(pkt->tx_rb, &crc.crc, 1);
#endif /* LWPKT_CFG_USE_CRC */

    /* Stop byte */
    b = LWPKT_STOP_BYTE;
    lwrb_write(pkt->tx_rb, &b, 1);

    /* Final step to notify app */
    SEND_EVT(pkt, LWPKT_EVT_WRITE);              /* Send write event */

    return lwpktOK;
}

/**
 * \brief           Reset packet state
 * \param[in]       pkt: Packet instance
 * \return          \ref lwpktOK on success, member of \ref lwpktr_t otherwise
 */
lwpktr_t
lwpkt_reset(lwpkt_t* pkt) {
    if (!LWPKT_IS_VALID(pkt)) {
        return lwpktERR;
    }
    LWPKT_RESET(pkt);
    return lwpktOK;
}

#if LWPKT_CFG_USE_EVT || __DOXYGEN__

/**
 * \brief           Set event function for packet events
 * \param[in]       pkt: Packet structure
 * \param[in]       evt_fn: Function pointer for events
 * \return          \ref lwpktOK on success, member of \ref lwpktr_t otherwise
 */
lwpktr_t
lwpkt_set_evt_fn(lwpkt_t* pkt, lwpkt_evt_fn evt_fn) {
    pkt->evt_fn = evt_fn;

    return lwpktOK;
}

#endif /* LWPKT_CFG_USE_EVT || __DOXYGEN__ */
