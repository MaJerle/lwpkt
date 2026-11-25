"""Lightweight packet protocol (LwPKT) - Python implementation.

Pure-Python encoder/decoder for the LwPKT wire protocol, mirroring the reference
C implementation at https://github.com/MaJerle/lwpkt.
"""
import collections
import enum
import time
import zlib
from typing import Callable, Final

START_BYTE: Final[int] = 0xAA
STOP_BYTE: Final[int] = 0x55

# Default broadcast address, same as LWPKT_CFG_ADDR_BROADCAST in the C library
ADDR_BROADCAST: Final[int] = 0xFF

# Largest value a from/to/flags/cmd/len field can carry (uint32_t in the C library)
_U32_MAX: Final[int] = 0xFFFFFFFF


def _build_crc8_table() -> tuple[int, ...]:
    """Lookup table for the reflected CRC-8 (poly 0x8C) used by the C library."""
    table = []
    for i in range(256):
        crc = i
        for _ in range(8):
            crc = (crc >> 1) ^ 0x8C if crc & 0x01 else crc >> 1
        table.append(crc)
    return tuple(table)


_CRC8_TABLE: Final[tuple[int, ...]] = _build_crc8_table()


class LwPKTError(Exception):
    """Base class for all lwpkt packet-decoding errors."""


class LwPKTLengthError(LwPKTError):
    """Decoded 'len' field exceeds max_data_len, or its encoding used too many bytes."""


class LwPKTFieldOverflowError(LwPKTError):
    """A from/to/cmd/flags field used too many continuation bytes, or exceeds 32 bits."""


class LwPKTCRCError(LwPKTError):
    """Computed CRC does not match the CRC received on the wire."""


class LwPKTStopByteError(LwPKTError):
    """Byte at the expected STOP position was not the stop marker."""


class _State(enum.Enum):
    """Packet receive state machine states."""
    START = 0
    FROM = 1
    TO = 2
    CMD = 3
    FLAGS = 4
    LEN = 5
    DATA = 6
    CRC = 7
    STOP = 8
    END = 9

class LwPKT(object):
    """LwPKT protocol instance: encodes outgoing packets and decodes incoming bytes."""

    class Packet(object):
        """A single in-progress or fully decoded LwPKT packet."""

        State = _State

        def __init__(self) -> None:
            self.state: _State = _State.START
            self.len: int = 0
            self.data: bytearray = bytearray()
            self.pkt_from: int = 0
            self.pkt_to: int = 0
            self.flags: int = 0
            self.cmd: int = 0
            self.crc: int = 0
            self.crc_recv: int = 0
            self.index: int = 0

        def _go_to_state(self, state: _State) -> None:
            """Reset the per-field index and move to a new parser state."""
            self.index = 0
            self.state = state

        def __repr__(self) -> str:
            return (f'LwPKT.Packet(state={self.state.name}, from=0x{self.pkt_from:X}, '
                    f'to=0x{self.pkt_to:X}, flags=0x{self.flags:X}, cmd=0x{self.cmd:X}, '
                    f'data={bytes(self.data)!r})')

    # Continuation-byte cap for from/to/cmd/flags, same as the C library's
    # LWPKT_VAR_LEN_MAX_ENC_BYTES(0xFFFFFFFF)
    _FIELD_MAX_ENC_BYTES: Final[int] = 5

    def __init__(self) -> None:
        """Create a new LwPKT protocol instance with all features enabled by default."""
        self.opt_addr: bool = True
        self.opt_addr_ext: bool = True
        self.opt_cmd: bool = True
        self.opt_cmd_ext: bool = True
        self.opt_crc: bool = True
        self.opt_crc32: bool = True
        self.opt_flags: bool = True
        self.our_addr: int = 0
        self.addr_broadcast: int = ADDR_BROADCAST

        # Max accepted data length, same default as LWPKT_CFG_MAX_DATA_LEN. 0 means unlimited.
        self.max_data_len: int = 256

        # A packet still in progress after this many seconds without new RX bytes is dropped,
        # same as LWPKT_CFG_PROCESS_INPROG_TIMEOUT. 0 disables the timeout.
        self.inprog_timeout: float = 0.1

        # Time source in seconds, replaceable for tests
        self.clock: Callable[[], float] = time.monotonic

        # Raw RX blocks as (receive time, bytes), each write_rx_data() call appends one
        self.rx_data: collections.deque[tuple[float, bytes]] = collections.deque()

        # Packet currently being received, replaced by a new object for each packet
        self.rx: LwPKT.Packet = LwPKT.Packet()

        # Fully received valid packets, waiting for rx_get_packet()
        self.rx_packets: collections.deque[LwPKT.Packet] = collections.deque()

        # Called with each decode error (the packet is already dropped). If it raises, the
        # exception propagates out of rx_process() and the remaining bytes are kept.
        self.on_error: Callable[[LwPKTError], None] | None = None

        # Rest of a block whose on_error callback raised, resumed on the next rx_process() call
        self._pending: tuple[float, bytes] | None = None

        # Time the last RX byte was written, used for the in-progress timeout
        self._last_rx_time: float = 0.0

        # Statistics
        self.total_rx_bytes: int = 0
        self.timeouts: int = 0
        self.error_counts: collections.Counter[type[LwPKTError]] = collections.Counter()
        self.last_error: LwPKTError | None = None

    def generate_packet(self, data: bytes | None = None, cmd: int = 0, addr_to: int = 0,
                        flags: int = 0) -> bytes:
        """Encode a single outgoing packet.

        Args:
            data: Payload bytes to include, or None/empty bytes for no payload.
            cmd: Command field value (only used if `opt_cmd` is enabled).
            addr_to: Destination address (only used if `opt_addr` is enabled).
            flags: Custom flags value (only used if `opt_flags` is enabled).

        Returns:
            The fully encoded packet, including start byte, CRC, and stop byte.

        Raises:
            ValueError: a field does not fit its wire encoding (0..0xFF for non-extended
                address/command, 0..0xFFFFFFFF otherwise), or the payload is longer than
                `max_data_len`.
        """
        data = bytes(data) if data else b''
        if self.max_data_len > 0 and len(data) > self.max_data_len:
            raise ValueError(f'Data length {len(data)} exceeds max_data_len={self.max_data_len}')

        data_out = bytearray((START_BYTE,))

        # Add address
        if self.opt_addr:
            if self.opt_addr_ext:
                data_out += self._varint_encode(self._check_range('our_addr', self.our_addr, _U32_MAX))
                data_out += self._varint_encode(self._check_range('addr_to', addr_to, _U32_MAX))
            else:
                data_out.append(self._check_range('our_addr', self.our_addr, 0xFF))
                data_out.append(self._check_range('addr_to', addr_to, 0xFF))

        # Add custom flags
        if self.opt_flags:
            data_out += self._varint_encode(self._check_range('flags', flags, _U32_MAX))

        # Add command
        if self.opt_cmd:
            if self.opt_cmd_ext:
                data_out += self._varint_encode(self._check_range('cmd', cmd, _U32_MAX))
            else:
                data_out.append(self._check_range('cmd', cmd, 0xFF))

        # Add data length, then actual data
        data_out += self._varint_encode(self._check_range('len(data)', len(data), _U32_MAX))
        data_out += data

        # CRC covers everything except the start byte, sent LSB first
        if self.opt_crc:
            crc = self._crc_update(0, data_out[1:])
            data_out += crc.to_bytes(self._crc_len(), 'little')

        # Packet ends here
        data_out.append(STOP_BYTE)
        return bytes(data_out)

    def write_rx_data(self, data: bytes) -> None:
        """Enqueue raw bytes received from the transport for later processing.

        Args:
            data: Raw bytes as received (e.g. from a serial port read).
        """
        self.rx_data.append((self.clock(), bytes(data)))

    def rx_process(self) -> bool:
        """Process previously-written RX data.

        Data must be previously written using write_rx_data(). Consumes all currently
        queued blocks, running each byte through the receive state machine.

        Malformed input does not stop processing, same as the C library: the packet in
        progress is dropped, the error is counted in `error_counts` (keyed by exception
        class), stored in `last_error` and passed to `on_error` if set, and parsing
        continues with the next byte. The possible errors are:

            LwPKTLengthError: the 'len' field is too large or used too many encoded bytes.
            LwPKTFieldOverflowError: a from/to/cmd/flags field used too many continuation
                bytes or exceeds 32 bits.
            LwPKTCRCError: the computed CRC does not match the CRC received on the wire.
            LwPKTStopByteError: the byte at the expected STOP position was wrong.
            LwPKTError: an internal parser state was reached that should be unreachable.

        A packet that is still incomplete after `inprog_timeout` seconds without new
        bytes is dropped, so one corrupted byte cannot stall reception indefinitely.

        Returns:
            True if at least one valid packet was queued during this call, False otherwise.
            Use rx_get_packet() to retrieve queued packets.

        Raises:
            Whatever `on_error` raises. Packets queued so far stay queued and the unprocessed
            bytes are kept for the next rx_process() call.
        """
        ret = False

        while True:
            if self._pending is not None:
                rx_time, block = self._pending
                self._pending = None
            elif self.rx_data:
                rx_time, block = self.rx_data.popleft()
                self.total_rx_bytes += len(block)
            else:
                break

            if not block:
                continue
            self._rx_check_timeout(rx_time)
            self._last_rx_time = rx_time

            # Parse the block and 
            # upon failure, store the last known position and re-raise the exception
            ret |= self._rx_parse(block, rx_time)

        self._rx_check_timeout(self.clock())
        return ret

    def rx_get_packet(self) -> 'LwPKT.Packet | None':
        """Retrieve the next fully decoded packet, if any.

        Returns:
            The next `Packet` ready to be read, or `None` if none is available.
        """
        return self.rx_packets.popleft() if self.rx_packets else None

    def reset(self) -> None:
        """Drop the packet currently being received and wait for a new start byte.

        Already received packets and queued RX bytes are kept (same as `lwpkt_reset`).
        """
        self.rx = LwPKT.Packet()

    def is_for_me(self, packet: 'LwPKT.Packet') -> bool:
        """Check if a received packet is addressed to `our_addr` (same as `lwpkt_is_for_me`)."""
        return packet.pkt_to == self.our_addr

    def is_broadcast(self, packet: 'LwPKT.Packet') -> bool:
        """Check if a received packet is sent to the broadcast address (`lwpkt_is_broadcast`)."""
        return packet.pkt_to == self.addr_broadcast

    def _rx_parse(self, block: bytes, rx_time: float) -> bool:
        """Run one block through the receive state machine.

        Args:
            block: Received bytes.
            rx_time: Time the block was written, kept with any unprocessed bytes.

        Returns:
            True if at least one valid packet was queued.

        Raises:
            Whatever the on_error callback raises, unchanged. The bytes of the block that
            were not processed yet are saved in self._pending for the next rx_process() call.
        """
        ret = False
        n = len(block)
        i = 0
        while i < n:
            ch = block[i]
            i += 1
            rx = self.rx
            try:
                match rx.state:
                    case _State.START:
                        if ch == START_BYTE:
                            self.rx = LwPKT.Packet()
                            self._rx_go_to_next_state()

                    case _State.FROM:
                        rx.crc = self._crc_update(rx.crc, block[i - 1:i])
                        if self._rx_varint(ch, 'pkt_from', '"from"', self.opt_addr_ext):
                            self._rx_go_to_next_state()

                    case _State.TO:
                        rx.crc = self._crc_update(rx.crc, block[i - 1:i])
                        if self._rx_varint(ch, 'pkt_to', '"to"', self.opt_addr_ext):
                            self._rx_go_to_next_state()

                    case _State.FLAGS:
                        rx.crc = self._crc_update(rx.crc, block[i - 1:i])
                        if self._rx_varint(ch, 'flags', '"flags"', True):
                            self._rx_go_to_next_state()

                    case _State.CMD:
                        rx.crc = self._crc_update(rx.crc, block[i - 1:i])
                        if self._rx_varint(ch, 'cmd', '"cmd"', self.opt_cmd_ext):
                            self._rx_go_to_next_state()

                    case _State.LEN:
                        rx.crc = self._crc_update(rx.crc, block[i - 1:i])
                        max_len = self.max_data_len if self.max_data_len > 0 else _U32_MAX
                        if self._rx_varint(ch, 'len', '"len"', True, max_len, LwPKTLengthError):
                            self._rx_go_to_next_state()

                    case _State.DATA:
                        # Take as much of the payload as this block holds in one slice
                        chunk = block[i - 1:i - 1 + rx.len - len(rx.data)]
                        rx.data += chunk
                        rx.crc = self._crc_update(rx.crc, chunk)
                        i += len(chunk) - 1
                        if len(rx.data) == rx.len:
                            self._rx_go_to_next_state()

                    case _State.CRC:
                        rx.crc_recv |= ch << (8 * rx.index)
                        rx.index += 1
                        # Check as soon as the last CRC byte arrives, before the stop byte,
                        # same as the C library
                        if rx.index == self._crc_len():
                            if rx.crc != rx.crc_recv:
                                raise LwPKTCRCError(
                                    f'CRC mismatch: expected 0x{rx.crc:X}, got 0x{rx.crc_recv:X}')
                            self._rx_go_to_next_state()

                    case _State.STOP:
                        if ch != STOP_BYTE:
                            raise LwPKTStopByteError(
                                f'Expected stop byte 0x{STOP_BYTE:02X}, got 0x{ch:02X}')

                        rx._go_to_state(_State.END)
                        self.rx_packets.append(rx)
                        self.rx = LwPKT.Packet()
                        ret = True

                    # Handle default situation - should be unreachable
                    case _:
                        raise LwPKTError(f'Unexpected internal parser state: {rx.state!r}')
            except LwPKTError as e:
                self.reset()
                self.error_counts[type(e)] += 1
                self.last_error = e
                if self.on_error is not None:
                    try:
                        self.on_error(e)
                    except BaseException:
                        self._pending = (rx_time, block[i:])
                        raise
        return ret

    def _rx_varint(self, ch: int, attr: str, name: str, extended: bool,
                   max_val: int = _U32_MAX,
                   error: type[LwPKTError] = LwPKTFieldOverflowError) -> bool:
        """Accumulate one byte of a variable-length field of self.rx.

        Args:
            ch: Received byte.
            attr: Name of the Packet attribute that holds the field value.
            name: Field name for error messages.
            extended: False for a fixed single-byte field.
            max_val: Largest value the field may decode to.
            error: Exception class raised when the field is invalid.

        Returns:
            True when the field is complete.
        """
        rx = self.rx
        if not extended:
            setattr(rx, attr, ch)
            return True

        value = getattr(rx, attr) | ((ch & 0x7F) << (7 * rx.index))
        setattr(rx, attr, value)
        rx.index += 1
        if (ch & 0x80) == 0:
            if value > max_val:
                raise error(f'{name} field value {value} exceeds maximum {max_val}')
            return True
        max_bytes = self._max_enc_bytes(max_val)
        if rx.index >= max_bytes:
            raise error(f'{name} field used too many continuation bytes (> {max_bytes})')
        return False

    def _rx_check_timeout(self, now: float) -> None:
        """Drop a packet in progress if no RX bytes arrived for `inprog_timeout` seconds."""
        if (self.inprog_timeout and self.rx.state is not _State.START
                and now - self._last_rx_time >= self.inprog_timeout):
            self.reset()
            self.timeouts += 1

    def _crc_len(self) -> int:
        """Number of CRC bytes on the wire."""
        return 4 if self.opt_crc32 else 1

    def _crc_update(self, crc: int, data: bytes | bytearray) -> int:
        """Fold bytes into a running CRC, starting from 0.

        CRC-32 uses zlib, which has the same parameters as the C library (reflected
        0x04C11DB7, init and final XOR 0xFFFFFFFF), so the value is final after every call.
        CRC-8 is the reflected 0x8C polynomial with init 0 and no final XOR.
        """
        if self.opt_crc32:
            return zlib.crc32(data, crc)
        table = _CRC8_TABLE
        for byt in data:
            crc = table[crc ^ byt]
        return crc

    @staticmethod
    def _check_range(name: str, value: int, max_val: int) -> int:
        """Return value if it fits 0..max_val, raise ValueError otherwise."""
        if not 0 <= value <= max_val:
            raise ValueError(f'{name}={value} out of range 0..0x{max_val:X}')
        return value

    @staticmethod
    def _varint_encode(num: int) -> bytearray:
        """Encode a non-negative integer as a 7-bit variable-length byte sequence, LSB group first."""
        if num < 0:
            raise ValueError(f'Cannot encode negative value {num}')
        out = bytearray()
        while True:
            val = num & 0x7F
            num >>= 7
            out.append(val | 0x80 if num else val)
            if num == 0:
                break
        return out

    @staticmethod
    def _max_enc_bytes(max_val: int) -> int:
        """Number of 7-bit encoded bytes needed to represent max_val.

        Mirrors the C library's LWPKT_VAR_LEN_MAX_ENC_BYTES macro: used to cap how many
        continuation bytes a variable-length encoded field may consume, so a corrupted
        stream can never leave the parser accumulating an unbounded number of bytes.

        Args:
            max_val: Largest value that ever needs to be representable.

        Returns:
            Number of 7-bit bytes needed (minimum 1).
        """
        n = 1
        v = max_val >> 7
        while v > 0:
            n += 1
            v >>= 7
        return n

    def _rx_go_to_next_state(self) -> None:
        """Advance self.rx to the next state, according to the configuration options."""
        new_state = _State.END
        match self.rx.state:
            case _State.START:
                if self.opt_addr: new_state = _State.FROM
                elif self.opt_flags: new_state = _State.FLAGS
                elif self.opt_cmd: new_state = _State.CMD
                else: new_state = _State.LEN
            case _State.TO:
                if self.opt_flags: new_state = _State.FLAGS
                elif self.opt_cmd: new_state = _State.CMD
                else: new_state = _State.LEN
            case _State.FLAGS:
                if self.opt_cmd: new_state = _State.CMD
                else: new_state = _State.LEN
            case _State.CMD:
                new_state = _State.LEN
            case _State.LEN:
                if self.rx.len > 0: new_state = _State.DATA
                elif self.opt_crc: new_state = _State.CRC
                else: new_state = _State.STOP
            case _State.DATA:
                if self.opt_crc: new_state = _State.CRC
                else: new_state = _State.STOP
            case _State.CRC:
                new_state = _State.STOP
            case _State.FROM:
                # There is no other way to go than into TO state
                new_state = _State.TO
            case _State.STOP:
                new_state = _State.START
            case _:
                new_state = _State.START
        if new_state != _State.END:
            self.rx._go_to_state(new_state)

