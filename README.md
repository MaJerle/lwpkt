# Packet protocol manager

<h3>Read first: <a href="http://docs.majerle.eu/projects/lwpkt/">Documentation</a></h3>

## Features

* Written in C (C11), compatible with ``size_t`` for size data types
* Platform independent, no architecture specific code
* Uses *LwRB* library for data read/write operations
* Optimized for embedded systems, allows high optimization for data transfer
* Support for events on packet ready, read or write operation
* Configurable settings for packet structure and variable data length
* Allows multiple notes in network with `from` and `to` addresses
* Separate optional field for *command* data type
* Variable data length to support theoretically unlimited packet length
* CRC check to handle data transmission errors
* User friendly MIT license

## Applications

To name a few:

* Communication in RS-485 network between various devices
* Low-level point to point packet communication (UART, USB, ethernet, ...)

## Contribute

Fresh contributions are always welcome. Simple instructions to proceed:

1. Fork Github repository
2. Follow [C style & coding rules](https://github.com/MaJerle/c-code-style) already used in the project
3. Create a pull request to develop branch with new features or bug fixes

Alternatively you may:

1. Report a bug
2. Ask for a feature request
