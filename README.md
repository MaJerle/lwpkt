# Packet protocol manager

LwPKT is a generic packet protocol library optimized for embedded systems.

[Open documentation](https://docs.majerle.eu/projects/lwpkt/)

## Features

* Written in C (C11), compatible with `stdint.h` data types
* Platform independent, no architecture specific code
* Uses [LwRB](https://github.com/MaJerle/lwrb) library for data read/write operations
* Support for events on packet-ready, read and write operations
* Optimized for embedded systems, allows high optimization for data transfer
* Configurable packet structure with support for variable-length data, theoretically unlimited size
* Allows multiple nodes in network with `from` and `to` addresses
* Separate optional field for *command* data type
* CRC-8 or CRC-32 check to handle data transmission errors, selectable at compile time
* Runtime toggle of address, command, flags and CRC features per packet instance
* Optional extended (variable-length) address and command encoding for larger identifier ranges
* Hardened parsing with bounded variable-length fields, rejecting malformed or oversized packets
* Python implementation available, published to PyPI as `lwpkt`
* User friendly MIT license

## Applications

To name a few:

* Communication in RS-485 network between various devices
* Low-level point to point packet communication (UART, USB, ethernet, ...)

## Contribute

Fresh contributions are always welcome. Simple instructions to proceed:

1. Fork Github repository
2. Follow [C style & coding rules](https://github.com/MaJerle/c-code-style) and use `clang-format` to format the code
3. Create a pull request to `develop` branch with new features or bug fixes

Alternatively you may:

1. Report a bug
2. Ask for a feature request
