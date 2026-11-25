# LwPKT (Python)

Pure-Python encoder/decoder for the [LwPKT](https://github.com/MaJerle/lwpkt) lightweight packet
protocol, mirroring the reference C implementation. Read the full protocol documentation at
[docs.majerle.eu/projects/lwpkt](https://docs.majerle.eu/projects/lwpkt/).

## Install

```sh
pip install lwpkt
```

## Usage

```python
from lwpkt import LwPKT

pkt = LwPKT()
pkt.our_addr = 0x01
pkt.on_error = lambda err: print("Malformed packet, discarded:", err)  # optional

# Encode an outgoing packet
data = pkt.generate_packet(b"hello", cmd=0x02, addr_to=0x02)

# Feed received bytes in and process them
pkt.write_rx_data(data)
if pkt.rx_process():
    while (received := pkt.rx_get_packet()) is not None:
        print(received.data, received.cmd, received.pkt_from)
```

Like the C library, `rx_process()` does not stop on malformed input. It drops the packet being
received, records the error and continues with the next byte, so valid packets before and after
a bad one are still decoded. Each error is a typed `LwPKTError` subclass (`LwPKTLengthError`,
`LwPKTFieldOverflowError`, `LwPKTCRCError`, `LwPKTStopByteError`):

- `error_counts` counts errors per exception class, e.g. `pkt.error_counts[LwPKTCRCError]`.
- `last_error` holds the most recent one.
- `on_error`, if set, is called with each one. If it raises, the exception propagates out of
  `rx_process()`, and the bytes not yet processed are kept for the next call.

The CRC is checked as soon as the CRC bytes arrive, before the stop byte, same as the C library.

`rx_get_packet()` returns `None` when no packet is waiting. Use `is_for_me(packet)` and
`is_broadcast(packet)` to filter by destination address, and `reset()` to drop a packet that
is being received.

`generate_packet()` raises `ValueError` when a field does not fit its wire encoding
(0..0xFF for non-extended address/command, 0..0xFFFFFFFF otherwise) or the payload is longer
than `max_data_len`.

### Defaults matching the C library

| Attribute        | Default | C option                           |
|------------------|---------|------------------------------------|
| `max_data_len`   | `256`   | `LWPKT_CFG_MAX_DATA_LEN` (0 = no limit)  |
| `inprog_timeout` | `0.1` s | `LWPKT_CFG_PROCESS_INPROG_TIMEOUT` (0 = off) |
| `addr_broadcast` | `0xFF`  | `LWPKT_CFG_ADDR_BROADCAST`         |

A packet that is still incomplete after `inprog_timeout` seconds without new bytes is dropped,
so a corrupted length byte cannot block reception. If your application reads the serial port in
batches slower than this, increase the timeout.

## Tests

```sh
pip install -e .[test]
pytest
```

`tests/c_vectors/vectors.json` holds packets encoded by the C library for every option
combination; the tests check the Python encoder produces identical bytes and decodes them.
Regenerate with the command at the top of `tests/c_vectors/gen_vectors.c`.

## License

MIT, see [LICENSE](LICENSE).
