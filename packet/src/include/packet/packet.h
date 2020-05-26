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

/**
 * \brief           Packet result enumeration
 */
typedef enum {
    pktOK = 0x00,                               /*!< Function returns successfully */
    pktERR,                                     /*!< General error for function status */
    pktINPROG,                                  /*!< Receive is in progress */
    pktVALID,                                   /*!< packet valid and ready to be read as CRC is valid and STOP received */
    pktERRCRC,                                  /*!< CRC integrity error for the packet. Will not wait STOP byte if received */
    pktERRSTOP,                                 /*!< Packet error with STOP byte, wrong character received for STOP */
    pktWAITDATA,                                /*!< Packet state is in start mode, waiting start byte to start receiving */
                                                
} pktr_t;

/**
 * \brief           CRC structure for packet
 */
typedef struct {
    uint8_t crc;
} pkt_crc_t;

/**
 * \brief           Packet structure
 */
typedef struct {
    uint8_t addr;                               /* Current device address */
    uint8_t data[PKT_MAX_DATA_LEN];             /* Memory to write received data */

    struct {
        pkt_state_t state;                      /*!< Actual packet state machine */
        pkt_crc_t crc;                          /*!< Packet CRC byte */
        uint8_t from;                           /*!< Device address packet is coming from */
        uint8_t to;                             /*!< Device address packet is intended for */
        uint8_t cmd;                            /*!< Command packet */
        size_t len;                             /*!< Number of bytes to receive */
        size_t index;                           /*!< General index variable for multi-byte parts of packet */
    } m;                                        /*!< Module that is periodically reset for next packet */
} pkt_t;

pktr_t  pkt_init(pkt_t* pkt, uint8_t addr);
pktr_t  pkt_set_addr(pkt_t* pkt, uint8_t addr);
pktr_t  pkt_read(pkt_t* pkt, RINGBUFF_VOLATILE ringbuff_t* rx_rb);
pktr_t  pkt_write(pkt_t* pkt, RINGBUFF_VOLATILE ringbuff_t* tx_rb, uint8_t to, uint8_t cmd, const void* data, size_t len);
pktr_t  pkt_reset(pkt_t* pkt);

#define pkt_get_from_addr(pkt)              ((pkt)->m.from)
#define pkt_get_to_addr(pkt)                ((pkt)->m.to)
#define pkt_get_data(pkt)                   ((pkt)->data)
#define pkt_get_data_len(pkt)               ((pkt)->m.len)
#define pkt_get_cmd(pkt)                    ((pkt)->m.cmd)
#define pkt_is_for_me(pkt)                  ((pkt)->m.to == (pkt)->addr)

/**
 * \}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PACKET_PROTOCOL_HDR_H */
