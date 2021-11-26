/**
 * \file            lwpkt_opt.h
 * \brief           LwPKT options
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
#ifndef LWPKT_HDR_OPT_H
#define LWPKT_HDR_OPT_H

/* Uncomment to ignore user options (or set macro in compiler flags) */
/* #define LWPKT_IGNORE_USER_OPTS */

/* Include application options */
#ifndef LWPKT_IGNORE_USER_OPTS
#include "lwpkt_opts.h"
#endif /* LWPKT_IGNORE_USER_OPTS */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \defgroup        LWPKT_OPT Configuration
 * \brief           Default configuration setup
 * \{
 */

/**
 * \brief           Maximum length of `data` part of the packet in units of bytes
 *
 */
#ifndef LWPKT_CFG_MAX_DATA_LEN
#define LWPKT_CFG_MAX_DATA_LEN                  256
#endif

/**
 * \brief           Address identifying broadcast message to all devices
 *
 */
#ifndef LWPKT_CFG_ADDR_BROADCAST
#define LWPKT_CFG_ADDR_BROADCAST                0xFF
#endif

/**
 * \brief           Enables `1` or disables `0` `from` and `to` fields in the protocol.
 *
 * This features is useful if communication is between 2 devices exclusively,
 * without addressing requirements
 */
#ifndef LWPKT_CFG_USE_ADDR
#define LWPKT_CFG_USE_ADDR                      1
#endif

/**
 * \brief           Enables `1` or disables `0` extended address length.
 *
 * When enabled, multi-byte addresses are supported with MSB codification.
 * Maximum address is limited to `32-bits`.
 *
 * When disabled, simple `8-bit` address is fixed with single byte.
 *
 * Feature is disabled by default to keep architecture compatibility
 */
#ifndef LWPKT_CFG_ADDR_EXTENDED
#define LWPKT_CFG_ADDR_EXTENDED                 0
#endif

/**
 * \brief           Enables `1` or disables `0` `cmd` field in the protocol.
 *
 * When disabled, command part is not used
 */
#ifndef LWPKT_CFG_USE_CMD
#define LWPKT_CFG_USE_CMD                       1
#endif

/**
 * \brief           Enables `1` or disables `0` CRC check in the protocol.
 *
 */
#ifndef LWPKT_CFG_USE_CRC
#define LWPKT_CFG_USE_CRC                       1
#endif

/**
 * \brief           Defines timeout time before packet is considered as not valid
 *                  when too long time in data-read mode
 *
 * Used with \ref lwpkt_process function
 */
#ifndef LWPKT_CFG_PROCESS_INPROG_TIMEOUT
#define LWPKT_CFG_PROCESS_INPROG_TIMEOUT        100
#endif

/**
 * \brief           Enables `1` or disables `0` event functions for read and write operations
 *
 */
#ifndef LWPKT_CFG_USE_EVT
#define LWPKT_CFG_USE_EVT                       1
#endif

/**
 * \}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LWPKT_HDR_OPT_H */
