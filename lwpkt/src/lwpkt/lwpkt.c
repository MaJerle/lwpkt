/**
 * \file            lwpkt.c
 * \brief           Lightweight packet protocol
 */

/*
 * Copyright (c) 2024 Tilen MAJERLE
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
#include <stdint.h>
#include <string.h>
#include "lwpkt/lwpkt.h"
#include "lwrb/lwrb.h"

#define LWPKT_IS_VALID(p) ((p) != NULL)
#define LWPKT_SET_STATE(p, s)                                                                                          \
    do {                                                                                                               \
        (p)->m.state = (s);                                                                                            \
        (p)->m.index = 0;                                                                                              \
    } while (0)
#define LWPKT_RESET(p)                                                                                                 \
    do {                                                                                                               \
        LWPKT_MEMSET(&(p)->m, 0x00, sizeof((p)->m));                                                                   \
        (p)->m.state = LWPKT_STATE_START;                                                                              \
    } while (0)

/* Start and STOP bytes definition */
#define LWPKT_START_BYTE 0xAA
#define LWPKT_STOP_BYTE  0x55

#if LWPKT_CFG_USE_CRC
#define WRITE_WITH_CRC(pkt, crc, tx_rb, b, len)                                                                        \
    do {                                                                                                               \
        lwrb_write((tx_rb), (b), (len));                                                                               \
        if (CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_USE_CRC, LWPKT_FLAG_USE_CRC)) {                           \
            prv_crc_in((crc), (b), (len));                                                                             \
        }                                                                                                              \
    } while (0)
#define ADD_IN_TO_CRC(pkt, crc, val, len)                                                                              \
    do {                                                                                                               \
        if (CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_USE_CRC, LWPKT_FLAG_USE_CRC)) {                           \
            prv_crc_in((crc), (val), (len));                                                                           \
        }                                                                                                              \
    } while (0)
#define INIT_CRC(pkt, crc) prv_crc_init((crc))
#else /* LWPKT_CFG_USE_CRC */
#define WRITE_WITH_CRC(pkt, crc, tx_rb, b, len)                                                                        \
    do {                                                                                                               \
        lwrb_write((tx_rb), (b), (len));                                                                               \
    } while (0)
#define ADD_IN_TO_CRC(pkt, crc, val, len)
#define INIT_CRC(pkt, crc)
#endif /* !LWPKT_CFG_USE_CRC */

#if LWPKT_CFG_USE_EVT
#define SEND_EVT(p, t)                                                                                                 \
    if ((p)->evt_fn != NULL) {                                                                                         \
        (p)->evt_fn((p), (t));                                                                                         \
    }
#else /* LWPKT_CFG_USE_EVT */
#define SEND_EVT(p, t)                                                                                                 \
    do {                                                                                                               \
        (void)(p);                                                                                                     \
        (void)(t);                                                                                                     \
    } while (0)
#endif /* !LWPKT_CFG_USE_EVT */

/* Flags for dynamically settable features in the library */
#define LWPKT_FLAG_USE_CRC       ((uint8_t)0x01)
#define LWPKT_FLAG_USE_ADDR      ((uint8_t)0x02)
#define LWPKT_FLAG_USE_CMD       ((uint8_t)0x04)
#define LWPKT_FLAG_ADDR_EXTENDED ((uint8_t)0x08)

/* Checks if feature is enabled for specific pkt instance */
#define CHECK_FEATURE_CONFIG_MODE_ENABLED(_pkt_, _feature_, _flag_)                                                    \
    (0                                                    /* For alignment purpose only */                             \
     || ((_feature_) == 1)                                /* 1 == feature is globally enabled */                       \
     || ((_feature_) == 2 && ((_pkt_)->flags & (_flag_))) /* 2 == feature is dynamically enabled */                    \
    )

/* Calculates number of bytes required to encode length. Result is increased for num_var (+= operand)*/
#define CALC_BYTES_NUM_FOR_LEN(num_var, len_var)                                                                       \
    do {                                                                                                               \
        uint32_t len_local = (len_var);                                                                                \
        do {                                                                                                           \
            ++(num_var);                                                                                               \
            len_local >>= 7;                                                                                           \
        } while (len_local > 0);                                                                                       \
    } while (0)

#if LWPKT_CFG_USE_CRC || __DOXYGEN__

/**
 * \brief           Add new value to CRC instance
 * \param[in]       crcobj: CRC instance
 * \param[in]       inp: Input data in byte format
 * \param[in]       len: Number of bytes to process
 * \return          Current CRC calculated value after all bytes or `0` on error input data
 */
static uint8_t
prv_crc_in(lwpkt_crc_t* crcobj, const void* inp, const size_t len) {
    const uint8_t* p_data = inp;

    if (crcobj == NULL || p_data == NULL || len == 0) {
        return 0;
    }

    for (size_t i = 0; i < len; ++i, ++p_data) {
        uint8_t inbyte = *p_data;
        for (uint8_t j = 8U; j > 0; --j) {
            uint8_t mix = (uint8_t)(crcobj->crc ^ inbyte) & 0x01U;
            crcobj->crc >>= 1U;
            if (mix > 0) {
                crcobj->crc ^= 0x8CU;
            }
            inbyte >>= 0x01U;
        }
    }
    return crcobj->crc;
}

/**
 * \brief           Initialize CRC instance to default values
 * \param[in]       crcobj: CRC instance
 */
static void
prv_crc_init(lwpkt_crc_t* crcobj) {
    LWPKT_MEMSET(crcobj, 0x00, sizeof(*crcobj));
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
lwpkt_init(lwpkt_t* pkt, lwrb_t* tx_rb, lwrb_t* rx_rb) {
    if (pkt == NULL) {
        return lwpktERR;
    }

    LWPKT_MEMSET(pkt, 0x00, sizeof(*pkt));
    LWPKT_RESET(pkt);

    pkt->tx_rb = tx_rb;
    pkt->rx_rb = rx_rb;
    pkt->flags |= 0xFF;

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

    SEND_EVT(pkt, LWPKT_EVT_PRE_READ);

    /* Process bytes from RX ringbuffer */
    /* Read byte by byte and go through state machine */
    while (lwrb_read(pkt->rx_rb, &b, 1) == 1) {
        e = 1;
        switch (pkt->m.state) {
            case LWPKT_STATE_START: {
                if (b == LWPKT_START_BYTE) {
                    LWPKT_RESET(pkt); /* Reset instance and make it ready for receiving */
                    INIT_CRC(pkt, &pkt->m.crc);

                    /*
                     * 1. Go to addressing, if addressing is enabled.
                     * 2. Go to command parser, if it is enabled
                     * 3. Go to data length otherwise (always enabled)
                     */
                    LWPKT_SET_STATE(pkt,
                                    CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_USE_ADDR, LWPKT_FLAG_USE_ADDR)
                                        ? LWPKT_STATE_FROM
                                        : (CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_USE_CMD, LWPKT_FLAG_USE_CMD)
                                               ? LWPKT_STATE_CMD
                                               : LWPKT_STATE_LEN));
                }
                break;
            }
#if LWPKT_CFG_USE_ADDR
            case LWPKT_STATE_FROM: {
                ADD_IN_TO_CRC(pkt, &pkt->m.crc, &b, 1);

                if (0) {
#if LWPKT_CFG_ADDR_EXTENDED
                } else if (CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_ADDR_EXTENDED, LWPKT_FLAG_ADDR_EXTENDED)) {
                    pkt->m.from |= (uint8_t)(b & 0x7FU) << ((size_t)7U * (size_t)pkt->m.index++);
#endif /* LWPKT_CFG_ADDR_EXTENDED */
                } else {
                    pkt->m.from = b;
                }

                /* Check if ready to move forward */
                if (!LWPKT_CFG_ADDR_EXTENDED /* Default mode goes straight with single byte */
                    || (LWPKT_CFG_ADDR_EXTENDED && (b & 0x80U) == 0x00)) { /* Extended mode must have MSB set to 0 */
                    LWPKT_SET_STATE(pkt, LWPKT_STATE_TO);
                }
                break;
            }
            case LWPKT_STATE_TO: {
                ADD_IN_TO_CRC(pkt, &pkt->m.crc, &b, 1);

                if (0) {
#if LWPKT_CFG_ADDR_EXTENDED
                } else if (CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_ADDR_EXTENDED, LWPKT_FLAG_ADDR_EXTENDED)) {
                    pkt->m.to |= (uint8_t)(b & 0x7FU) << ((size_t)7U * (size_t)pkt->m.index++);
#endif /* !LWPKT_CFG_ADDR_EXTENDED */
                } else {
                    pkt->m.to = b;
                }

                /* Check if ready to move forward */
                if (!CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_ADDR_EXTENDED, LWPKT_FLAG_ADDR_EXTENDED)
                    || (b & 0x80U) == 0x00) { /* Extended mode must have MSB set to 0 */
                    LWPKT_SET_STATE(pkt, CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_USE_CMD, LWPKT_FLAG_USE_CMD)
                                             ? LWPKT_STATE_CMD
                                             : LWPKT_STATE_LEN);
                }
                break;
            }
#endif /* LWPKT_CFG_USE_ADDR */
#if LWPKT_CFG_USE_CMD
            case LWPKT_STATE_CMD: {
                pkt->m.cmd = b;
                ADD_IN_TO_CRC(pkt, &pkt->m.crc, &b, 1);
                LWPKT_SET_STATE(pkt, LWPKT_STATE_LEN);
                break;
            }
#endif /* LWPKT_CFG_USE_CMD */
            case LWPKT_STATE_LEN: {
                pkt->m.len |= (b & 0x7FU) << ((size_t)7U * (size_t)pkt->m.index++);
                ADD_IN_TO_CRC(pkt, &pkt->m.crc, &b, 1U);

                /* Last length bytes has MSB bit set to 0 */
                if ((b & 0x80U) == 0) {
                    if (pkt->m.len == 0) {
                        LWPKT_SET_STATE(pkt,
                                        CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_USE_CRC, LWPKT_FLAG_USE_CRC)
                                            ? LWPKT_STATE_CRC
                                            : LWPKT_STATE_STOP);
                    } else {
                        LWPKT_SET_STATE(pkt, LWPKT_STATE_DATA);
                    }
                }
                break;
            }
            case LWPKT_STATE_DATA: {
                if (pkt->m.index < sizeof(pkt->data)) {
                    pkt->data[pkt->m.index++] = b;
                    ADD_IN_TO_CRC(pkt, &pkt->m.crc, &b, 1U);
                    if (pkt->m.index == pkt->m.len) {
                        LWPKT_SET_STATE(pkt,
                                        CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_USE_CRC, LWPKT_FLAG_USE_CRC)
                                            ? LWPKT_STATE_CRC
                                            : LWPKT_STATE_STOP);
                    }
                } else {
                    LWPKT_RESET(pkt);
                    res = lwpktERRMEM;
                    goto retpre;
                }
                break;
            }
#if LWPKT_CFG_USE_CRC
            case LWPKT_STATE_CRC: {
                ADD_IN_TO_CRC(pkt, &pkt->m.crc, &b, 1U);
                if (pkt->m.crc.crc == 0) {
                    LWPKT_SET_STATE(pkt, LWPKT_STATE_STOP);
                } else {
                    LWPKT_RESET(pkt);
                    res = lwpktERRCRC;
                    goto retpre;
                }
                break;
            }
#endif /* LWPKT_CFG_USE_CRC */
            case LWPKT_STATE_STOP: {
                LWPKT_SET_STATE(pkt, LWPKT_STATE_START); /* Reset packet state */
                if (b == LWPKT_STOP_BYTE) {
                    res = lwpktVALID; /* Packet fully valid, take data from it */
                    goto retpre;
                } else {
                    res = lwpktERRSTOP; /* Packet is missing STOP byte! */
                    goto retpre;
                }
            }
            default: {
                LWPKT_RESET(pkt);
                res = lwpktERR; /* Hard error */
                goto retpre;
            }
        }
    }
    if (pkt->m.state == LWPKT_STATE_START) {
        res = lwpktWAITDATA;
    } else {
        res = lwpktINPROG;
    }
retpre:
    SEND_EVT(pkt, LWPKT_EVT_POST_READ);
    if (e) {
        SEND_EVT(pkt, LWPKT_EVT_READ); /* Send read event */
    }
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
    lwpktr_t pktres = lwpktERR;

    if (pkt == NULL) {
        return pktres;
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
    return pktres;
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
    lwpktr_t res = lwpktOK;
#if LWPKT_CFG_USE_CRC
    lwpkt_crc_t crc;
#endif /* LWPKT_CFG_USE_CRC */
#if LWPKT_CFG_ADDR_EXTENDED
    lwpkt_addr_t addr;
#endif /* LWPKT_CFG_ADDR_EXTENDED */
    size_t org_len = len;
    uint8_t b;

    SEND_EVT(pkt, LWPKT_EVT_PRE_WRITE);

    if (!LWPKT_IS_VALID(pkt)) {
        res = lwpktERR;
        goto fast_return;
    } else {
        /* Check for required memory for packet */
        size_t min_mem = 2U;

        /* Addresses */
#if LWPKT_CFG_USE_ADDR
        if (CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_USE_ADDR, LWPKT_FLAG_USE_ADDR)) {
            if (0) {
#if LWPKT_CFG_ADDR_EXTENDED
                /* Dynamic configuration? */
            } else if (CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_ADDR_EXTENDED, LWPKT_FLAG_ADDR_EXTENDED)) {
                CALC_BYTES_NUM_FOR_LEN(min_mem, pkt->addr);
                CALC_BYTES_NUM_FOR_LEN(min_mem, to);
#endif /* !LWPKT_CFG_ADDR_EXTENDED */
            } else {
                min_mem += 2U; /* Static configuration */
            }
        }
#endif /* LWPKT_CFG_USE_ADDR */

#if LWPKT_CFG_USE_CMD
        if (CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_USE_CMD, LWPKT_FLAG_USE_CMD)) {
            ++min_mem; /* CMD part */
        }
#endif /* LWPKT_CFG_USE_CMD */

        /* Encode data length number + add actual data space requirement */
        CALC_BYTES_NUM_FOR_LEN(min_mem, len);
        min_mem += len; /* Data length */

#if LWPKT_CFG_USE_CRC
        if (CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_USE_CRC, LWPKT_FLAG_USE_CRC)) {
            ++min_mem; /* CRC part */
        }
#endif /* LWPKT_CFG_USE_CRC */

        /* Verify enough memory */
        if (lwrb_get_free(pkt->tx_rb) < min_mem) {
            res = lwpktERRMEM;
            goto fast_return;
        }
    }

#if LWPKT_CFG_USE_CRC
    if (CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_USE_CRC, LWPKT_FLAG_USE_CRC)) {
        prv_crc_init(&crc);
    }
#endif /* LWPKT_CFG_USE_CRC */

    /* Start byte */
    b = LWPKT_START_BYTE;
    lwrb_write(pkt->tx_rb, &b, 1U);

#if LWPKT_CFG_USE_ADDR
    /* Add addresses */
    if (CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_USE_ADDR, LWPKT_FLAG_USE_ADDR)) {
        if (0) {
#if LWPKT_CFG_ADDR_EXTENDED
        } else if (CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_ADDR_EXTENDED, LWPKT_FLAG_ADDR_EXTENDED)) {
            /* FROM address */
            addr = pkt->addr;
            do {
                b = (addr & 0x7FU) | (addr > 0x7FU ? 0x80U : 0);
                WRITE_WITH_CRC(pkt, &crc, pkt->tx_rb, &b, 1);
                addr >>= 7;
            } while (addr > 0);

            /* TO address */
            addr = to;
            do {
                b = (addr & 0x7FU) | (addr > 0x7FU ? 0x80U : 0);
                WRITE_WITH_CRC(pkt, &crc, pkt->tx_rb, &b, 1);
                addr >>= 7;
            } while (addr > 0);
        } else {
#endif /* !LWPKT_CFG_ADDR_EXTENDED */
            WRITE_WITH_CRC(pkt, &crc, pkt->tx_rb, &pkt->addr, 1);
            WRITE_WITH_CRC(pkt, &crc, pkt->tx_rb, &to, 1);
        }
    }
#endif /* LWPKT_CFG_USE_ADDR */

#if LWPKT_CFG_USE_CMD
    /* CMD byte */
    if (CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_USE_CMD, LWPKT_FLAG_USE_CMD)) {
        WRITE_WITH_CRC(pkt, &crc, pkt->tx_rb, &cmd, 1);
    }
#endif /* LWPKT_CFG_USE_CMD */

    /* Length bytes */
    do {
        b = (len & 0x7FU) | (len > 0x7FU ? 0x80U : 0);
        WRITE_WITH_CRC(pkt, &crc, pkt->tx_rb, &b, 1);
        len >>= 7U;
    } while (len > 0);

    /* Data bytes, but only if length more than 0 */
    if (org_len > 0) {
        WRITE_WITH_CRC(pkt, &crc, pkt->tx_rb, data, org_len);
    }

#if LWPKT_CFG_USE_CRC
    /* CRC byte */
    if (CHECK_FEATURE_CONFIG_MODE_ENABLED(pkt, LWPKT_CFG_USE_CRC, LWPKT_FLAG_USE_CRC)) {
        lwrb_write(pkt->tx_rb, &crc.crc, 1);
    }
#endif /* LWPKT_CFG_USE_CRC */

    /* Stop byte */
    b = LWPKT_STOP_BYTE;
    lwrb_write(pkt->tx_rb, &b, 1U);

fast_return:
    /* Final step to notify app */
    SEND_EVT(pkt, LWPKT_EVT_POST_WRITE); /* Release write event */
    if (res == lwpktOK) {
        SEND_EVT(pkt, LWPKT_EVT_WRITE); /* Send write event */
    }
    return res;
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

#if LWPKT_CFG_USE_CRC == 2 || __DOXYGEN__

/**
 * \brief           Set CRC mode enabled.
 * 
 * \note            This function is only available, if \ref LWPKT_CFG_USE_CRC is `2`
 * \param           pkt: LwPKT instance
 * \param           enable: `1` to enable, `0` otherwise
 */
void
lwpkt_set_crc_enabled(lwpkt_t* pkt, uint8_t enable) {
    if (enable) {
        pkt->flags |= LWPKT_FLAG_USE_CRC;
    } else {
        pkt->flags &= ~LWPKT_FLAG_USE_CRC;
    }
}

#endif /* LWPKT_CFG_USE_CRC == 2 || __DOXYGEN__ */

#if LWPKT_CFG_USE_ADDR == 2 || __DOXYGEN__

/**
 * \brief           Enable addressing in the packet
 * 
 * \note            This function is only available, if \ref LWPKT_CFG_USE_ADDR is `2`
 * \param           pkt: LwPKT instance
 * \param           enable: `1` to enable, `0` otherwise
 */
void
lwpkt_set_addr_enabled(lwpkt_t* pkt, uint8_t enable) {
    if (enable) {
        pkt->flags |= LWPKT_FLAG_USE_ADDR;
    } else {
        pkt->flags &= ~LWPKT_FLAG_USE_ADDR;
    }
}

#endif /* LWPKT_CFG_USE_ADDR == 2 || __DOXYGEN__ */

#if LWPKT_CFG_ADDR_EXTENDED == 2 || __DOXYGEN__

/**
 * \brief           Enable extended addressing in the packet
 * 
 * \note            This function is only available, if \ref LWPKT_CFG_ADDR_EXTENDED is `2`
 * \param           pkt: LwPKT instance
 * \param           enable: `1` to enable, `0` otherwise
 */
void
lwpkt_set_addr_extended_enabled(lwpkt_t* pkt, uint8_t enable) {
    if (enable) {
        pkt->flags |= LWPKT_FLAG_ADDR_EXTENDED;
    } else {
        pkt->flags &= ~LWPKT_FLAG_ADDR_EXTENDED;
    }
}

#endif /* LWPKT_CFG_ADDR_EXTENDED == 2 || __DOXYGEN__ */

#if LWPKT_CFG_USE_CMD == 2 || __DOXYGEN__

/**
 * \brief           Enable CMD mode in the packet
 * 
 * \note            This function is only available, if \ref LWPKT_CFG_USE_CMD is `2`
 * \param           pkt: LwPKT instance
 * \param           enable: `1` to enable, `0` otherwise
 */
void
lwpkt_set_cmd_enabled(lwpkt_t* pkt, uint8_t enable) {
    if (enable) {
        pkt->flags |= LWPKT_FLAG_USE_CMD;
    } else {
        pkt->flags &= ~LWPKT_FLAG_USE_CMD;
    }
}

#endif /* LWPKT_CFG_USE_CMD == 2 || __DOXYGEN__ */
