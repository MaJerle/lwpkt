"""Tests for the lwpkt Python package.

Covers the original round-trip sweep across every static feature combination
(ported from the library's former ``if __name__ == '__main__':`` self-test),
plus the hardening added against malformed/corrupted input: byte-count caps
on every variable-length field, the max_data_len check, and CRC/stop-byte
validation - all of which must be recorded as a typed LwPKTError subclass
(without stopping the parser) rather than hang or silently corrupt state.
"""
import json
import pathlib
import zlib

import pytest

from lwpkt import (
    LwPKT,
    LwPKTCRCError,
    LwPKTError,
    LwPKTFieldOverflowError,
    LwPKTLengthError,
    LwPKTStopByteError,
    ADDR_BROADCAST,
    START_BYTE,
)


def assert_rx_error(pkt: LwPKT, error: type, valid: bool = False) -> None:
    """rx_process() must not raise, and must record exactly one error of the given type."""
    assert pkt.rx_process() is valid
    assert type(pkt.last_error) is error
    assert sum(pkt.error_counts.values()) == 1
    assert pkt.error_counts[error] == 1
    assert pkt.rx.state is LwPKT.Packet.State.START


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
    assert decoded is not None

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
    assert_rx_error(pkt, LwPKTLengthError)


def test_len_runaway_continuation_bytes_never_reaches_zero_payload() -> None:
    """All-zero-payload continuation bytes on 'len' must still be bounded by byte count.

    This is the case a naive "reject if decoded value is too large" check cannot catch:
    the decoded value stays 0 forever while the continuation bit stays set, so only a cap
    on the number of bytes consumed (not on the value) can terminate this safely.
    """
    pkt = make_pkt()
    pkt.max_data_len = 256
    pkt.write_rx_data(bytes([START_BYTE] + [0x80] * 20))
    assert_rx_error(pkt, LwPKTLengthError)


def test_len_runaway_continuation_bytes_unlimited_max_data_len() -> None:
    """Same runaway case must still be bounded even when max_data_len is 0 (unlimited)."""
    pkt = make_pkt()
    pkt.max_data_len = 0
    pkt.write_rx_data(bytes([START_BYTE] + [0x80] * 20))
    assert_rx_error(pkt, LwPKTLengthError)


@pytest.mark.parametrize("field_opts,byte_offset", [
    ({"opt_addr": True, "opt_addr_ext": True}, 1),  # FROM is the first field after START
    ({"opt_flags": True}, 1),
    ({"opt_cmd": True, "opt_cmd_ext": True}, 1),
])
def test_field_overflow_raises(field_opts: dict, byte_offset: int) -> None:
    """Runaway continuation bytes on from/flags/cmd raise LwPKTFieldOverflowError."""
    pkt = make_pkt(**field_opts)
    pkt.write_rx_data(bytes([START_BYTE] + [0x80] * 20))
    assert_rx_error(pkt, LwPKTFieldOverflowError)


def test_field_overflow_to() -> None:
    """Runaway continuation bytes on 'to' (after a valid 'from') raise LwPKTFieldOverflowError."""
    pkt = make_pkt(opt_addr=True, opt_addr_ext=True)
    # Valid single-byte "from" (0x01, MSB clear), then runaway "to" continuation bytes.
    pkt.write_rx_data(bytes([START_BYTE, 0x01] + [0x80] * 20))
    assert_rx_error(pkt, LwPKTFieldOverflowError)


def test_crc_error() -> None:
    """A corrupted CRC byte raises LwPKTCRCError."""
    pkt = make_pkt(opt_crc=True, opt_crc32=True)
    packet = bytearray(pkt.generate_packet(b"x"))
    packet[-2] ^= 0xFF  # corrupt last CRC byte, just before the stop byte
    pkt.write_rx_data(bytes(packet))
    assert_rx_error(pkt, LwPKTCRCError)


def test_stop_byte_error() -> None:
    """A wrong byte at the stop position raises LwPKTStopByteError."""
    pkt = make_pkt()
    packet = bytearray(pkt.generate_packet(b"x"))
    packet[-1] = 0x00
    pkt.write_rx_data(bytes(packet))
    assert_rx_error(pkt, LwPKTStopByteError)


def test_batch_valid_then_invalid_preserves_first_packet() -> None:
    """An error on packet 2 does not hide packet 1 decoded earlier in the same call."""
    pkt = make_pkt()
    good = bytes(pkt.generate_packet(b"hi"))
    bad = bytes([START_BYTE] + [0x80] * 20)
    pkt.write_rx_data(good + bad)

    assert_rx_error(pkt, LwPKTLengthError, valid=True)
    decoded = pkt.rx_get_packet()
    assert decoded is not None
    assert bytes(decoded.data) == b"hi"


def test_recovery_after_error() -> None:
    """After an error, the parser resyncs and decodes a subsequent valid packet."""
    pkt = make_pkt()
    pkt.write_rx_data(bytes([START_BYTE] + [0x80] * 20))
    assert_rx_error(pkt, LwPKTLengthError)

    pkt.write_rx_data(bytes(pkt.generate_packet(b"bye")))
    assert pkt.rx_process() is True
    decoded = pkt.rx_get_packet()
    assert decoded is not None
    assert bytes(decoded.data) == b"bye"


def test_recovery_after_error_same_block() -> None:
    """Parsing continues past an error within the same block, in the same call."""
    pkt = make_pkt()
    bad = bytes([START_BYTE] + [0x80] * 20)
    good = bytes(pkt.generate_packet(b"next"))
    pkt.write_rx_data(bad + good)

    assert_rx_error(pkt, LwPKTLengthError, valid=True)
    decoded = pkt.rx_get_packet()
    assert decoded is not None
    assert bytes(decoded.data) == b"next"


def test_errors_counted_per_type() -> None:
    pkt = make_pkt(opt_crc=True)
    crc_bad = bytearray(pkt.generate_packet(b"x"))
    crc_bad[-2] ^= 0xFF
    stop_bad = bytearray(pkt.generate_packet(b"y"))
    stop_bad[-1] = 0x00
    pkt.write_rx_data(bytes(crc_bad) + bytes(stop_bad) + bytes(crc_bad) + pkt.generate_packet(b"ok"))
    assert pkt.rx_process() is True
    assert pkt.error_counts == {LwPKTCRCError: 2, LwPKTStopByteError: 1}
    assert bytes(pkt.rx_get_packet().data) == b"ok"


def test_on_error_callback() -> None:
    pkt = make_pkt()
    seen = []
    pkt.on_error = seen.append
    pkt.write_rx_data(bytes([START_BYTE] + [0x80] * 20) + pkt.generate_packet(b"z"))
    assert pkt.rx_process() is True
    assert len(seen) == 1 and isinstance(seen[0], LwPKTLengthError)


def test_on_error_raising_keeps_remaining_bytes() -> None:
    """An exception from on_error propagates; the rest of the block is processed next call."""
    pkt = make_pkt()

    def boom(err: LwPKTError) -> None:
        raise RuntimeError("stop") from err

    pkt.on_error = boom
    bad = bytes([START_BYTE] + [0x80] * 20)
    pkt.write_rx_data(pkt.generate_packet(b"a") + bad + pkt.generate_packet(b"b"))
    with pytest.raises(RuntimeError):
        pkt.rx_process()
    assert bytes(pkt.rx_get_packet().data) == b"a"

    pkt.on_error = None
    assert pkt.rx_process() is True
    assert bytes(pkt.rx_get_packet().data) == b"b"
    assert pkt.total_rx_bytes == len(pkt.generate_packet(b"a")) * 2 + len(bad)


def test_crc_checked_before_stop_byte() -> None:
    """Like C, a bad CRC is reported as soon as the CRC bytes arrive, even if STOP is also bad."""
    pkt = make_pkt(opt_crc=True, opt_crc32=True)
    packet = bytearray(pkt.generate_packet(b"x"))
    packet[-2] ^= 0xFF
    packet[-1] = 0x00
    pkt.write_rx_data(bytes(packet))
    assert_rx_error(pkt, LwPKTCRCError)


def test_crc_error_reported_without_stop_byte() -> None:
    """The CRC error does not wait for the stop byte to arrive."""
    pkt = make_pkt(opt_crc=True)
    packet = bytearray(pkt.generate_packet(b"x"))
    packet[-2] ^= 0xFF
    pkt.write_rx_data(bytes(packet[:-1]))
    assert_rx_error(pkt, LwPKTCRCError)


# ---------------------------------------------------------------------------
# Golden vectors produced by the reference C library (see c_vectors/gen_vectors.c)
# ---------------------------------------------------------------------------

_C_VECTORS = json.loads((pathlib.Path(__file__).parent / "c_vectors" / "vectors.json").read_text())
_C_PAYLOADS = [b"", b"Hello World\r\n", bytes(range(200))]


def _pkt_from_vector(vec: dict) -> LwPKT:
    opts = vec["opts"]
    pkt = LwPKT()
    pkt.opt_addr = bool(opts & 0x01)
    pkt.opt_addr_ext = bool(opts & 0x02)
    pkt.opt_flags = bool(opts & 0x04)
    pkt.opt_cmd = bool(opts & 0x08)
    pkt.opt_cmd_ext = bool(opts & 0x10)
    pkt.opt_crc = bool(opts & 0x20)
    pkt.opt_crc32 = bool(opts & 0x40)
    pkt.our_addr = vec["our_addr"]
    return pkt


@pytest.mark.parametrize("vec", _C_VECTORS, ids=lambda v: f"opts{v['opts']}-p{v['payload']}")
def test_encode_matches_c(vec: dict) -> None:
    """Python encoder output is byte-identical to the C library's lwpkt_write()."""
    pkt = _pkt_from_vector(vec)
    out = pkt.generate_packet(_C_PAYLOADS[vec["payload"]], cmd=vec["cmd"], addr_to=vec["to"],
                              flags=vec["flags"])
    assert isinstance(out, bytes)
    assert out.hex() == vec["packet"]


@pytest.mark.parametrize("split", [None, 1, 7])
@pytest.mark.parametrize("vec", _C_VECTORS, ids=lambda v: f"opts{v['opts']}-p{v['payload']}")
def test_decode_c_packets(vec: dict, split: int | None) -> None:
    """Packets made by the C library decode correctly, whole or split into small blocks."""
    pkt = _pkt_from_vector(vec)
    raw = bytes.fromhex(vec["packet"])
    if split is None:
        pkt.write_rx_data(raw)
    else:
        for i in range(0, len(raw), split):
            pkt.write_rx_data(raw[i:i + split])
    assert pkt.rx_process() is True
    decoded = pkt.rx_get_packet()
    assert decoded is not None
    assert pkt.rx_get_packet() is None
    if pkt.opt_addr:
        assert decoded.pkt_from == vec["our_addr"]
        assert decoded.pkt_to == vec["to"]
    if pkt.opt_flags:
        assert decoded.flags == vec["flags"]
    if pkt.opt_cmd:
        assert decoded.cmd == vec["cmd"]
    assert bytes(decoded.data) == _C_PAYLOADS[vec["payload"]]


# ---------------------------------------------------------------------------
# CRC
# ---------------------------------------------------------------------------

def _crc_bitwise(data: bytes, crc32: bool) -> int:
    """Bit-serial reference CRC, same algorithm as prv_crc_calc_one() in lwpkt.c."""
    crc = 0xFFFFFFFF if crc32 else 0
    poly = 0xEDB88320 if crc32 else 0x8C
    for byt in data:
        for _ in range(8):
            mix = (crc ^ byt) & 0x01
            crc >>= 1
            if mix:
                crc ^= poly
            byt >>= 1
    return crc ^ 0xFFFFFFFF if crc32 else crc


@pytest.mark.parametrize("crc32", [False, True])
def test_crc_matches_bitwise_reference(crc32: bool) -> None:
    """Table/zlib CRC equals the C library's bit-serial CRC, also when fed in pieces."""
    pkt = make_pkt(opt_crc=True, opt_crc32=crc32)
    data = bytes(range(256)) * 3
    assert pkt._crc_update(0, data) == _crc_bitwise(data, crc32)
    crc = 0
    for i in range(0, len(data), 5):
        crc = pkt._crc_update(crc, data[i:i + 5])
    assert crc == _crc_bitwise(data, crc32)
    if crc32:
        assert crc == zlib.crc32(data)


# ---------------------------------------------------------------------------
# Encoder range checks
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("kwargs", [
    {"cmd": -1},
    {"cmd": 1 << 32},
    {"addr_to": -5},
    {"addr_to": 1 << 32},
    {"flags": -1},
    {"flags": 1 << 32},
])
def test_encode_rejects_out_of_range_extended(kwargs: dict) -> None:
    """Negative or > 32-bit values raise instead of hanging or producing undecodable packets."""
    pkt = LwPKT()
    with pytest.raises(ValueError):
        pkt.generate_packet(b"x", **kwargs)


def test_encode_rejects_out_of_range_our_addr() -> None:
    pkt = LwPKT()
    pkt.our_addr = -1
    with pytest.raises(ValueError):
        pkt.generate_packet(b"x")


@pytest.mark.parametrize("kwargs", [{"cmd": 0x100}, {"addr_to": 0x100}])
def test_encode_rejects_out_of_range_single_byte(kwargs: dict) -> None:
    """Non-extended address/command must fit one byte."""
    pkt = make_pkt(opt_addr=True, opt_cmd=True)
    with pytest.raises(ValueError):
        pkt.generate_packet(b"x", **kwargs)


def test_encode_rejects_data_longer_than_max_data_len() -> None:
    pkt = LwPKT()
    pkt.max_data_len = 4
    with pytest.raises(ValueError):
        pkt.generate_packet(b"12345")
    pkt.max_data_len = 0
    assert pkt.generate_packet(b"12345")


# ---------------------------------------------------------------------------
# Decoder hardening
# ---------------------------------------------------------------------------

def test_default_max_data_len_matches_c() -> None:
    assert LwPKT().max_data_len == 256


@pytest.mark.parametrize("field_opts", [
    {"opt_addr": True, "opt_addr_ext": True},
    {"opt_flags": True},
    {"opt_cmd": True, "opt_cmd_ext": True},
])
def test_field_value_above_32_bits_raises(field_opts: dict) -> None:
    """A 5-byte field carrying more than 32 bits is rejected, C stores these as uint32_t."""
    pkt = make_pkt(**field_opts)
    pkt.write_rx_data(bytes([START_BYTE, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F]))  # 35-bit value
    assert_rx_error(pkt, LwPKTFieldOverflowError)


def test_field_value_exactly_32_bits_ok() -> None:
    pkt = make_pkt(opt_cmd=True, opt_cmd_ext=True)
    pkt.write_rx_data(pkt.generate_packet(None, cmd=0xFFFFFFFF))
    assert pkt.rx_process() is True
    assert pkt.rx_get_packet().cmd == 0xFFFFFFFF


def test_len_above_32_bits_unlimited_raises() -> None:
    pkt = make_pkt()
    pkt.max_data_len = 0
    pkt.write_rx_data(bytes([START_BYTE, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F]))
    assert_rx_error(pkt, LwPKTLengthError)


class FakeClock:
    def __init__(self) -> None:
        self.now = 1000.0

    def __call__(self) -> float:
        return self.now


def test_inprog_timeout_drops_stalled_packet() -> None:
    """A corrupted, huge length no longer swallows the following packets forever."""
    pkt = make_pkt()
    pkt.max_data_len = 0
    clock = FakeClock()
    pkt.clock = clock

    pkt.write_rx_data(bytes([START_BYTE, 0xFF, 0xFF, 0xFF, 0x7F]))  # len = 268435455
    assert pkt.rx_process() is False
    assert pkt.rx.state is LwPKT.Packet.State.DATA

    clock.now += 0.2
    pkt.write_rx_data(pkt.generate_packet(b"hi"))
    assert pkt.rx_process() is True
    assert pkt.timeouts == 1
    assert bytes(pkt.rx_get_packet().data) == b"hi"


def test_inprog_timeout_on_idle_call() -> None:
    """rx_process() without new data also drops a stale packet, like lwpkt_process()."""
    pkt = make_pkt()
    clock = FakeClock()
    pkt.clock = clock
    pkt.write_rx_data(bytes([START_BYTE, 0x05, 0x01]))
    pkt.rx_process()
    assert pkt.rx.state is LwPKT.Packet.State.DATA
    clock.now += 0.05
    pkt.rx_process()
    assert pkt.rx.state is LwPKT.Packet.State.DATA
    clock.now += 0.1
    pkt.rx_process()
    assert pkt.rx.state is LwPKT.Packet.State.START
    assert pkt.timeouts == 1


def test_inprog_timeout_not_triggered_by_slow_but_steady_bytes() -> None:
    pkt = make_pkt()
    clock = FakeClock()
    pkt.clock = clock
    for byt in pkt.generate_packet(b"abc"):
        clock.now += 0.05
        pkt.write_rx_data(bytes([byt]))
        pkt.rx_process()
    assert pkt.timeouts == 0
    assert bytes(pkt.rx_get_packet().data) == b"abc"


def test_inprog_timeout_disabled() -> None:
    pkt = make_pkt()
    pkt.inprog_timeout = 0
    clock = FakeClock()
    pkt.clock = clock
    pkt.write_rx_data(bytes([START_BYTE, 0x05, 0x01]))
    pkt.rx_process()
    clock.now += 100
    pkt.rx_process()
    assert pkt.rx.state is LwPKT.Packet.State.DATA


# ---------------------------------------------------------------------------
# Packet objects and helpers
# ---------------------------------------------------------------------------

def test_queued_packet_not_modified_by_parser() -> None:
    """A packet handed out by rx_get_packet() is no longer the parser's working object."""
    pkt = make_pkt()
    pkt.write_rx_data(pkt.generate_packet(b"a") + pkt.generate_packet(b"b"))
    assert pkt.rx_process() is True
    first = pkt.rx_get_packet()
    second = pkt.rx_get_packet()
    assert first is not second
    assert first.state is LwPKT.Packet.State.END
    assert bytes(first.data) == b"a"
    assert bytes(second.data) == b"b"
    assert pkt.rx is not first and pkt.rx is not second


def test_rx_get_packet_empty_returns_none() -> None:
    assert LwPKT().rx_get_packet() is None


def test_is_for_me_and_broadcast() -> None:
    tx = LwPKT()
    tx.our_addr = 0x10
    rx = LwPKT()
    rx.our_addr = 0x20

    rx.write_rx_data(tx.generate_packet(b"x", addr_to=0x20))
    rx.write_rx_data(tx.generate_packet(b"y", addr_to=ADDR_BROADCAST))
    rx.rx_process()
    direct, broadcast = rx.rx_get_packet(), rx.rx_get_packet()
    assert rx.is_for_me(direct) and not rx.is_broadcast(direct)
    assert rx.is_broadcast(broadcast) and not rx.is_for_me(broadcast)


def test_reset_drops_packet_in_progress() -> None:
    pkt = make_pkt()
    raw = pkt.generate_packet(b"hello")
    pkt.write_rx_data(raw[:4])
    pkt.rx_process()
    assert pkt.rx.state is not LwPKT.Packet.State.START
    pkt.reset()
    assert pkt.rx.state is LwPKT.Packet.State.START
    pkt.write_rx_data(raw)
    assert pkt.rx_process() is True


def test_total_rx_bytes_counts_each_byte_once() -> None:
    pkt = make_pkt()
    bad = bytes([START_BYTE] + [0x80] * 20)
    good = pkt.generate_packet(b"next")
    pkt.write_rx_data(bad + good)
    pkt.rx_process()
    pkt.rx_process()
    assert pkt.total_rx_bytes == len(bad) + len(good)


def test_on_error_exception_propagates_unchanged() -> None:
    """The callback's exception reaches the caller as-is, with its own __cause__ kept."""
    pkt = make_pkt()

    def boom(err: LwPKTError) -> None:
        raise RuntimeError("stop") from err

    pkt.on_error = boom
    pkt.write_rx_data(bytes([START_BYTE] + [0x80] * 20))
    with pytest.raises(RuntimeError) as exc_info:
        pkt.rx_process()
    assert isinstance(exc_info.value.__cause__, LwPKTLengthError)
