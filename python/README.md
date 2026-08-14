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
from lwpkt import LwPKT, LwPKTError

pkt = LwPKT()
pkt.our_addr = 0x01

# Encode an outgoing packet
data = pkt.generate_packet(b"hello", cmd=0x02, addr_to=0x02)

# Feed received bytes in and process them
pkt.write_rx_data(data)
try:
    if pkt.rx_process():
        received = pkt.rx_get_packet()
        print(received.data, received.cmd, received.pkt_from)
except LwPKTError as e:
    print("Malformed packet, discarded:", e)
```

`rx_process()` raises a typed exception (`LwPKTLengthError`, `LwPKTFieldOverflowError`,
`LwPKTCRCError`, `LwPKTStopByteError` - all subclasses of `LwPKTError`) when it encounters
malformed input. The parser resets itself before raising, so it's always safe to keep calling
`write_rx_data()` / `rx_process()` after catching one.

## License

MIT, see [LICENSE](LICENSE).
