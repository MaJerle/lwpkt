.. _um:

User manual
===========

LwPKT protocol library is a simple state-machine parser and raw data generator
to allow `2` or more devices in a network to communicate in a structure way.

It is perfectly suitable for communication in embedded systems, suchs as `RS-485`, where multiple
devices could be easily connected to one big network.

LwPKT library uses well known and easy implementation of `LwRB <https://github.com/MaJerle/lwpkt>`_ library
for data read and data write. It expects `2` different buffer instances.

Parser is simple state machine that reads and processes every received character from read buffer.
When application wants to transmit data, LwPKT library generates raw data and writes them to TX buffer.

Combination of both gives embedded applications freedom to implement communication protocols for TX and RX.

Packet structure
****************

Packet structure consists of several fields, where some are optional and some are mandatory.

.. figure:: ../static/images/packet-structure.svg
    :align: center
    :alt: Default packet structure

    Default packet structure

* ``START``: Byte with fixed value to represent start of packet
* ``FROM``: Byte(s) from where this packet is coming from. Optional field, can be disabled with :c:macro:`LWPKT_CFG_USE_ADDR`
* ``TO``: Byte(s) to where this packet is targeting. Optional field, can be disabled with :c:macro:`LWPKT_CFG_USE_ADDR`
* ``CMD``: Byte with optional command field to better align with multiple packets. Optional field, can be disabled with :c:macro:`LWPKT_CFG_USE_CMD`
* ``LEN``: Length of *data* part field. This is variable multi-byte length to support data length ``>= 256`` bytes. Always present
* ``DATA``: Optional data field. Number of bytes is as in ``LEN`` field
* ``CRC``: 8-bit CRC of all enabled fields except *START* and *STOP* bytes. Optional field, can be disabled with :c:macro:`LWPKT_CFG_USE_CRC`
* ``STOP``: Byte with fixed value to represent stop of packet

.. tip::
    If only ``2`` devices are communicating and are in the network, considering disabling :c:macro:`LWPKT_CFG_USE_ADDR` to improve
    data bandwidth and remove unnecessary packet overhead

Data input output
*****************

LwPKT library only reads and writes to ``2`` ringbuffers used for read and write operations.
It is up to application to implement how buffers are actually later written for read operation and sent out on the network for write operation.

.. warning::
    LwPKT is platform independant and requires final application to actually take care of data being read/written from/to ringbuffers and
    transferred further over the network

Variable data length
********************

Some fields implement variable data length feature, to optimize data transfer length.
Currently supported fields are:

* ``DATA`` field is always enabled
* ``FROM`` and ``TO`` fields when :c:macro:`LWPKT_CFG_ADDR_EXTENDED` feature is enabled

Variable data length is a feature that uses minimum number of bytes to transfer data.
It uses ``7 LSB bits`` per byte for actual data, and ``MSB`` bit to indicate if there are more bytes coming after.
For example, values between ``0x00 - 0x7F`` are codified within single byte, while values between ``0x80 - 0x3F`` require ``2`` bytes for transfer.
To transfer ``32-bit`` variable, minimum ``1-byte`` and maximum ``5-bytes`` are used.

.. tip ::
    Data codification is always LSB Byte first.

Event management
****************

LwPKT may operate in event mode, meaning that application receives notifications on different events:

* New packet has been received
* Timeout during packet receive

Timeout function is used when network doesn't transmit all bytes or if data got lost in the middle of transmission.
This is to make sure that packet protocol library easily recovers to be able to receive more packets in the future

.. warning::
    To use this feature, application must provide accurate timing in units of milliseconds
    to be able to properly handle timeout function.

.. literalinclude:: ../../examples/example_lwpkt_evt.c
    :language: c
    :linenos:
    :caption: LwPKT example with events

.. toctree::
    :maxdepth: 2
