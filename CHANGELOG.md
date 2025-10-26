# Changelog

## Develop

## v1.5.1

- Fix the platformio library package description

## v1.5.0

- Fix the platformio library package description
- Add the `lwpkt_set_arg` and `lwpkt_get_arg` functions for custom user arguments

## v1.4.1

- Fix the parameter check for write operation

## v1.4.0

- Add python file prototype.
- Add support for `CRC-32` with `LWPKT_CFG_CRC32` option
- Add support for extended command length (variable length), configurable through `LWPKT_CFG_CMD_EXTENDED`
- Rework library CMake with removed INTERFACE type, replaced by static library
- Added config flags `LWPKT_OFF`, `LWPKT_ON_STATIC` and `LWPKT_ON_DYNAMIC`

## v1.3.0

- Split CMakeLists.txt files between library and executable
- Change license year to 2022
- Add `.clang-format` draft
- Change license year to 2023
- Add memory overflow check
- Add more events for pre and post write & read operations
- Add more recent version of LwRB
- Fix compilation error if CRC mode is disabled
- Add support for dynamic configuration, to support multiple LwPKT instances in one project
- Add flags support to allow customer user flags in packet

## v1.2.0

- Added support for events on packet ready, read or write operation
- Add `library.json` for platform.io

## v1.1.0

- Added support for variable length for address fields

## v1.0.1

- Added sphinx documentation to the repository
- Improved code documentation for doxygen compliancy

## v1.0.0

- First stable release
