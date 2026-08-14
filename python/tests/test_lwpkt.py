"""Tests for the lwpkt Python package.

Covers the original round-trip sweep across every static feature combination
(ported from the library's former ``if __name__ == '__main__':`` self-test),
plus the hardening added against malformed/corrupted input: byte-count caps
on every variable-length field, the max_data_len check, and CRC/stop-byte
validation - all of which must raise a typed LwPKTError subclass rather than
hang or silently corrupt state.
"""
import pytest

from lwpkt import (
    LwPKT,
    LwPKTCRCError,
    LwPKTError,
    LwPKTFieldOverflowError,
    LwPKTLengthError,
    LwPKTStopByteError,
    START_BYTE,
)


def make_pkt(**opts: bool) -> LwPKT:
    """Create an LwPKT instance with all features off except those passed as True."""
    pkt = LwPKT()
    pkt.opt_addr = opts.get("opt_addr", False)
    pkt.opt_addr_ext = opts.get("opt_addr_ext", False)
    pkt.opt_flags = opts.get("opt_flags", False)
    pkt.opt_cmd = opts.get("opt_cmd", False)
    pkt.opt_cmd_ext = opts.get("opt_cmd_ext", False)
    pkt.opt_crc = opts.get("opt_crc", False)
    pkt.opt_crc32 = opts.get("opt_crc32", False)
    return pkt


@pytest.mark.parametrize("i", range(1 << 8))
def test_round_trip_sweep(i: int) -> None:
    """Every combination of static feature flags round-trips through encode/decode."""
    addr_to = 0x87654321
    addr_our = 0x12345678
    flags = 0xACCE550F
    cmd = 0x85542343

    pkt = LwPKT()
    pkt.opt_addr = bool(i & 0x01)
    pkt.opt_addr_ext = bool(i & 0x02)
    pkt.opt_flags = bool(i & 0x04)
    pkt.opt_cmd = bool(i & 0x08)
    pkt.opt_cmd_ext = bool(i & 0x10)
    pkt.opt_crc = bool(i & 0x20)
    pkt.opt_crc32 = bool(i & 0x40)
    data = bytearray(b"Hello World\r\n") if (i & 0x80) else None
    data_len = len(data) if data else 0

    if not pkt.opt_addr_ext:
        addr_to &= 0xFF
        addr_our &= 0xFF
    if not pkt.opt_cmd_ext:
        cmd &= 0xFF

    pkt.our_addr = addr_our

    packet = pkt.generate_packet(data, addr_to=addr_to, flags=flags, cmd=cmd)
    pkt.write_rx_data(packet)

    assert pkt.rx_process() is True
    decoded = pkt.rx_get_packet()
    assert decoded is not False

    if pkt.opt_addr:
        assert decoded.pkt_from == pkt.our_addr
        assert decoded.pkt_to == addr_to
    if pkt.opt_flags:
        assert decoded.flags == flags
    if pkt.opt_cmd:
        assert decoded.cmd == cmd
    assert decoded.len == data_len
    assert bytes(decoded.data) == bytes(data or b"")


def test_len_value_exceeds_max_data_len() -> None:
    """A well-formed but too-large 'len' field raises LwPKTLengthError."""
    pkt = make_pkt()
    pkt.max_data_len = 5
    pkt.write_rx_data(bytes([START_BYTE, 0x0A]))  # len=10, single byte, no continuation
    with pytest.raises(LwPKTLengthError):
        pkt.rx_process()


def test_len_runaway_continuation_bytes_never_reaches_zero_payload() -> None:
    """All-zero-payload continuation bytes on 'len' must still be bounded by byte count.

    This is the case a naive "reject if decoded value is too large" check cannot catch:
    the decoded value stays 0 forever while the continuation bit stays set, so only a cap
    on the number of bytes consumed (not on the value) can terminate this safely.
    """
    pkt = make_pkt()
    pkt.max_data_len = 256
    pkt.write_rx_data(bytes([START_BYTE] + [0x80] * 20))
    with pytest.raises(LwPKTLengthError):
        pkt.rx_process()


def test_len_runaway_continuation_bytes_unlimited_max_data_len() -> None:
    """Same runaway case must still be bounded even when max_data_len is 0 (unlimited)."""
    pkt = make_pkt()
    assert pkt.max_data_len == 0
    pkt.write_rx_data(bytes([START_BYTE] + [0x80] * 20))
    with pytest.raises(LwPKTLengthError):
        pkt.rx_process()


@pytest.mark.parametrize("field_opts,byte_offset", [
    ({"opt_addr": True, "opt_addr_ext": True}, 1),  # FROM is the first field after START
    ({"opt_flags": True}, 1),
    ({"opt_cmd": True, "opt_cmd_ext": True}, 1),
])
def test_field_overflow_raises(field_opts: dict, byte_offset: int) -> None:
    """Runaway continuation bytes on from/flags/cmd raise LwPKTFieldOverflowError."""
    pkt = make_pkt(**field_opts)
    pkt.write_rx_data(bytes([START_BYTE] + [0x80] * 20))
    with pytest.raises(LwPKTFieldOverflowError):
        pkt.rx_process()


def test_field_overflow_to() -> None:
    """Runaway continuation bytes on 'to' (after a valid 'from') raise LwPKTFieldOverflowError."""
    pkt = make_pkt(opt_addr=True, opt_addr_ext=True)
    # Valid single-byte "from" (0x01, MSB clear), then runaway "to" continuation bytes.
    pkt.write_rx_data(bytes([START_BYTE, 0x01] + [0x80] * 20))
    with pytest.raises(LwPKTFieldOverflowError):
        pkt.rx_process()


def test_crc_error() -> None:
    """A corrupted CRC byte raises LwPKTCRCError."""
    pkt = make_pkt(opt_crc=True, opt_crc32=True)
    packet = bytearray(pkt.generate_packet(b"x"))
    packet[-2] ^= 0xFF  # corrupt last CRC byte, just before the stop byte
    pkt.write_rx_data(bytes(packet))
    with pytest.raises(LwPKTCRCError):
        pkt.rx_process()


def test_stop_byte_error() -> None:
    """A wrong byte at the stop position raises LwPKTStopByteError."""
    pkt = make_pkt()
    packet = bytearray(pkt.generate_packet(b"x"))
    packet[-1] = 0x00
    pkt.write_rx_data(bytes(packet))
    with pytest.raises(LwPKTStopByteError):
        pkt.rx_process()


def test_batch_valid_then_invalid_preserves_first_packet() -> None:
    """An exception on packet 2 must not lose packet 1 decoded earlier in the same call."""
    pkt = make_pkt()
    good = bytes(pkt.generate_packet(b"hi"))
    bad = bytes([START_BYTE] + [0x80] * 20)
    pkt.write_rx_data(good + bad)

    with pytest.raises(LwPKTLengthError):
        pkt.rx_process()

    decoded = pkt.rx_get_packet()
    assert decoded is not False
    assert bytes(decoded.data) == b"hi"


def test_recovery_after_exception() -> None:
    """After an exception, the parser resyncs and decodes a subsequent valid packet."""
    pkt = make_pkt()
    bad = bytes([START_BYTE] + [0x80] * 20)
    pkt.write_rx_data(bad)
    with pytest.raises(LwPKTError):
        pkt.rx_process()

    pkt.write_rx_data(bytes(pkt.generate_packet(b"bye")))
    assert pkt.rx_process() is True
    decoded = pkt.rx_get_packet()
    assert decoded is not False
    assert bytes(decoded.data) == b"bye"


def test_recovery_after_exception_same_block() -> None:
    """Recovery also works when the good packet's tail was in the same original block
    as the bad packet - the carry-over buffer must not drop or reorder those bytes."""
    pkt = make_pkt()
    bad = bytes([START_BYTE] + [0x80] * 20)
    good = bytes(pkt.generate_packet(b"next"))
    pkt.write_rx_data(bad + good)

    with pytest.raises(LwPKTLengthError):
        pkt.rx_process()

    assert pkt.rx_process() is True
    decoded = pkt.rx_get_packet()
    assert decoded is not False
    assert bytes(decoded.data) == b"next"
