/**
 * \file            lwpkt_opt.h
 * \brief           LwPKT options
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
 * Version:         v1.4.1
 */
#ifndef LWPKT_OPT_HDR_H
#define LWPKT_OPT_HDR_H

/* Features options */
#define LWPKT_OFF        0
#define LWPKT_ON_STATIC  1
#define LWPKT_ON_DYNAMIC 2

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
 * \brief           Memory set function
 * 
 * \note            Function footprint is the same as \ref memset
 */
#ifndef LWPKT_MEMSET
#define LWPKT_MEMSET(dst, val, len) memset((dst), (val), (len))
#endif

/**
 * \brief           Memory copy function
 * 
 * \note            Function footprint is the same as \ref memcpy
 */
#ifndef LWPKT_MEMCPY
#define LWPKT_MEMCPY(dst, src, len) memcpy((dst), (src), (len))
#endif

/**
 * \brief           Maximum length of `data` part of the packet in units of bytes
 *
 */
#ifndef LWPKT_CFG_MAX_DATA_LEN
#define LWPKT_CFG_MAX_DATA_LEN 256
#endif

/**
 * \brief           Address identifying broadcast message to all devices
 *
 */
#ifndef LWPKT_CFG_ADDR_BROADCAST
#define LWPKT_CFG_ADDR_BROADCAST 0xFF
#endif

/**
 * \brief           Enables `> 0` or disables `0` `from` and `to` fields in the protocol.
 *
 * This features is useful if communication is between 2 devices exclusively,
 * without addressing requirements
 * 
 * Configuration options:
 *  - `LWPKT_OFF`: Feature is globally disabled in the library
 *  - `LWPKT_ON_STATIC`: Feature is globally enabled in the library
 *  - `LWPKT_ON_DYNAMIC`: Feature is dynamically enabled/disabled in the library, according to the LwPKT object instance.
 *      If set to `LWPKT_ON_DYNAMIC`, feature is by default enabled, but it can be disabled with appropriate API function.
 */
#ifndef LWPKT_CFG_USE_ADDR
#define LWPKT_CFG_USE_ADDR LWPKT_ON_STATIC
#endif

/**
 * \brief           Enables `> 0` or disables `0` extended address length.
 * \note            \ref LWPKT_CFG_USE_ADDR must be enabled for this feature to work
 *
 * When enabled, multi-byte variable length encoding is used for data storage.
 * Maximum address is limited to `32-bits`.
 *
 * When disabled, simple `8-bit` address is fixed with single byte.
 *
 * \note            Feature is disabled by default to keep architecture compatibility
 * 
 * Configuration options:
 *  - `LWPKT_OFF`: Feature is globally disabled in the library
 *  - `LWPKT_ON_STATIC`: Feature is globally enabled in the library
 *  - `LWPKT_ON_DYNAMIC`: Feature is dynamically enabled/disabled in the library, according to the LwPKT object instance.
 *      If set to `LWPKT_ON_DYNAMIC`, feature is by default enabled, but it can be disabled with appropriate API function.
 */
#ifndef LWPKT_CFG_ADDR_EXTENDED
#define LWPKT_CFG_ADDR_EXTENDED LWPKT_OFF
#endif

/**
 * \brief           Enables `> 0` or disables `0` `cmd` field in the protocol.
 *
 * When disabled, command part is not used
 * 
 * Configuration options:
 *  - `LWPKT_OFF`: Feature is globally disabled in the library
 *  - `LWPKT_ON_STATIC`: Feature is globally enabled in the library
 *  - `LWPKT_ON_DYNAMIC`: Feature is dynamically enabled/disabled in the library, according to the LwPKT object instance.
 *      If set to `LWPKT_ON_DYNAMIC`, feature is by default enabled, but it can be disabled with appropriate API function.
 */
#ifndef LWPKT_CFG_USE_CMD
#define LWPKT_CFG_USE_CMD LWPKT_ON_STATIC
#endif

/**
 * \brief           Enables `> 0` or disables `0` extended command length.
 * \note            \ref LWPKT_CFG_USE_CMD must be enabled for this feature to work
 *
 * When enabled, multi-byte command length is supported in the protocol and is variable length integer encoded.
 * 
 * When disabled, simple `8-bit` address is fixed with single byte.
 *
 * \note            Feature is disabled by default to keep architecture compatibility
 * 
 * Configuration options:
 *  - `LWPKT_OFF`: Feature is globally disabled in the library
 *  - `LWPKT_ON_STATIC`: Feature is globally enabled in the library
 *  - `LWPKT_ON_DYNAMIC`: Feature is dynamically enabled/disabled in the library, according to the LwPKT object instance.
 *      If set to `LWPKT_ON_DYNAMIC`, feature is by default enabled, but it can be disabled with appropriate API function.
 */
#ifndef LWPKT_CFG_CMD_EXTENDED
#define LWPKT_CFG_CMD_EXTENDED LWPKT_OFF
#endif

/**
 * \brief           Enables `> 0` or disables `0` CRC check in the protocol.
 * 
 * Configuration options:
 *  - `LWPKT_OFF`: Feature is globally disabled in the library
 *  - `LWPKT_ON_STATIC`: Feature is globally enabled in the library
 *  - `LWPKT_ON_DYNAMIC`: Feature is dynamically enabled/disabled in the library, according to the LwPKT object instance.
 *      If set to `LWPKT_ON_DYNAMIC`, feature is by default enabled, but it can be disabled with appropriate API function.
 */
#ifndef LWPKT_CFG_USE_CRC
#define LWPKT_CFG_USE_CRC LWPKT_ON_STATIC
#endif

/**
 * \brief           Enables `> 0` or disables `0` CRC-32 type.
 * \note            \ref LWPKT_CFG_USE_CRC must be enabled for this feature to work
 * 
 * It controls if CRC type is set to `8-bits` or `32-bits`
 * 
 * Configuration options:
 *  - `LWPKT_OFF`: CRC is ˙8-bits`, fixed value. 
 *  - `LWPKT_ON_STATIC`: Feature is globally enabled in the library and CRC is set fixed to `32-bits`
 *  - `LWPKT_ON_DYNAMIC`: Feature is dynamically enabled/disabled in the library, according to the LwPKT object instance.
 *      If set to `LWPKT_ON_DYNAMIC`, feature is by default enabled, but it can be disabled with appropriate API function.
 */
#ifndef LWPKT_CFG_CRC32
#define LWPKT_CFG_CRC32 LWPKT_OFF
#endif

/**
 * \brief           Enables `> 0` or disables `0` flags field in the protocol.
 * 
 * When enabled, multi-byte addresses are supported with MSB codification.
 * Maximum address is limited to `32-bits`.
 *
 * \note            Feature is disabled by default to keep architecture compatibility
 * 
 * Configuration options:
 *  - `LWPKT_OFF`: Feature is globally disabled in the library
 *  - `LWPKT_ON_STATIC`: Feature is globally enabled in the library
 *  - `LWPKT_ON_DYNAMIC`: Feature is dynamically enabled/disabled in the library, according to the LwPKT object instance.
 *      If set to `LWPKT_ON_DYNAMIC`, feature is by default enabled, but it can be disabled with appropriate API function.
 */
#ifndef LWPKT_CFG_USE_FLAGS
#define LWPKT_CFG_USE_FLAGS LWPKT_OFF
#endif

/**
 * \brief           Defines timeout time before packet is considered as not valid
 *                  when too long time in data-read mode
 *
 * Used with \ref lwpkt_process function
 */
#ifndef LWPKT_CFG_PROCESS_INPROG_TIMEOUT
#define LWPKT_CFG_PROCESS_INPROG_TIMEOUT 100
#endif

/**
 * \brief           Enables `1` or disables `0` event functions for read and write operations
 *
 */
#ifndef LWPKT_CFG_USE_EVT
#define LWPKT_CFG_USE_EVT 1
#endif

/**
 * \}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LWPKT_OPT_HDR_H */
