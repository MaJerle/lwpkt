LwPKT |version| documentation
=============================

Welcome to the documentation for version |version|.

LwPKT is a generic packet protocol library optimized for embedded systems.

.. image:: static/images/logo.svg
    :align: center

.. rst-class:: center
.. rst-class:: index_links

	:ref:`download_library` :ref:`getting_started` `Open Github <https://github.com/MaJerle/lwpkt>`_ `Donate <https://paypal.me/tilz0R>`_

Features
^^^^^^^^

* Written in ANSI C99, compatible with ``size_t`` for size data types
* Platform independent, no architecture specific code
* Uses `LwRB <https://github.com/MaJerle/lwrb>`_ library for data read/write operations
* Support for events on packet ready, read or write operation
* Optimized for embedded systems, allows high optimization for data transfer
* Configurable settings for packet structure and variable data length
* Allows multiple notes in network with `from` and `to` addresses
* Separate optional field for *command* data type
* Variable data length to support theoretically unlimited packet length
* CRC check to handle data transmission errors
* User friendly MIT license

Requirements
^^^^^^^^^^^^

* C compiler
* Few ``kB`` of non-volatile memory

Contribute
^^^^^^^^^^

Fresh contributions are always welcome. Simple instructions to proceed:

#. Fork Github repository
#. Respect `C style & coding rules <https://github.com/MaJerle/c-code-style>`_ used by the library
#. Create a pull request to ``develop`` branch with new features or bug fixes

Alternatively you may:

#. Report a bug
#. Ask for a feature request

License
^^^^^^^

.. literalinclude:: ../LICENSE

Table of contents
^^^^^^^^^^^^^^^^^

.. toctree::
    :maxdepth: 2
    :caption: Contents

    self
    get-started/index
    user-manual/index
    api-reference/index
    changelog/index

.. toctree::
    :maxdepth: 2
    :caption: Other projects
    :hidden:

    LwDTC - DateTimeCron <https://github.com/MaJerle/lwdtc>
    LwESP - ESP-AT library <https://github.com/MaJerle/lwesp>
    LwEVT - Event manager <https://github.com/MaJerle/lwevt>
    LwGPS - GPS NMEA parser <https://github.com/MaJerle/lwgps>
    LwGSM - GSM-AT library <https://github.com/MaJerle/lwgsm>
    LwJSON - JSON parser <https://github.com/MaJerle/lwjson>
    LwMEM - Memory manager <https://github.com/MaJerle/lwmem>
    LwOW - OneWire with UART <https://github.com/MaJerle/lwow>
    LwPKT - Packet protocol <https://github.com/MaJerle/lwpkt>
    LwPRINTF - Printf <https://github.com/MaJerle/lwprintf>
    LwRB - Ring buffer <https://github.com/MaJerle/lwrb>
    LwSHELL - Shell <https://github.com/MaJerle/lwshell>
    LwUTIL - Utility functions <https://github.com/MaJerle/lwutil>
