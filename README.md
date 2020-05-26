# Packet protocol manager

## Features

* Written in ANSI C99, compatible with ``size_t`` for size data types
* Implements simple protocol for (one target) RS-485 communication with multiple slaves
* Variable number of length bytes available to support packet longer than `255` bytes
* Supports CRC-8 integrity check
* Uses ringbuffers for TX and RX data, gives application full autonomy for data transmit or receive
* Supports manual packet state reset if timeout detected by application
* Operating system ready, thread-safe API
* User friendly MIT license

## Contribute

Fresh contributions are always welcome. Simple instructions to proceed::

1. Fork Github repository
2. Respect [C style & coding rules](https://github.com/MaJerle/c-code-style) used by the library
3. Create a pull request to develop branch with new features or bug fixes

Alternatively you may:

1. Report a bug
2. Ask for a feature request
