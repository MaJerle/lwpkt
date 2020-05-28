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

/**
 * \brief           Maximum length of `data` part of the packet in units of bytes
 */
#ifndef PKT_MAX_DATA_LEN
#define PKT_MAX_DATA_LEN                        256
#endif

/**
 * \brief           Address identifying broadcast message to all devices
 */
#ifndef PKT_ADDR_BROADCAST
#define PKT_ADDR_BROADCAST                      0xFF
#endif

/**
 * \brief           Packet state enumeration
 */
typedef enum {
    PKT_STATE_START = 0x00,                     /*!< Packet waits for start byte */
    PKT_STATE_FROM,                             /*!< Packet waits for "packet from" byte */
    PKT_STATE_TO,                               /*!< Packet waits for "packet to" byte */
    PKT_STATE_CMD,                              /*!< Packet waits for "packet cmd" byte */
    PKT_STATE_LEN,                              /*!< Packet waits for (multiple) data length bytes */
    PKT_STATE_DATA,                             /*!< Packet waits for actual data bytes */
    PKT_STATE_CRC,                              /*!< Packet waits for CRC data */
    PKT_STATE_STOP,                             /*!< Packet waits for stop byte */
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

#define pkt_get_from_addr(pkt)              (uint8_t)   (((pkt) != NULL) ? ((pkt)->m.from) : 0)
#define pkt_get_to_addr(pkt)                (uint8_t)   (((pkt) != NULL) ? ((pkt)->m.to) : 0)
#define pkt_get_data_len(pkt)               (size_t)    (((pkt) != NULL) ? ((pkt)->m.len) : 0)
#define pkt_get_data(pkt)                   (void *)    (((pkt) != NULL) ? ((pkt)->data) : NULL)
#define pkt_get_cmd(pkt)                    (uint8_t)   (((pkt) != NULL) ? ((pkt)->m.cmd) : 0)
#define pkt_is_for_me(pkt)                  (((pkt) != NULL) ? ((pkt)->m.to == (pkt)->addr) : 0)
#define pkt_is_broadcast(pkt)               (((pkt) != NULL) ? ((pkt)->m.to == PKT_ADDR_BROADCAST) : 0)

/**
 * \}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PACKET_PROTOCOL_HDR_H */
