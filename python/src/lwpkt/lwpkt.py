"""Lightweight packet protocol (LwPKT) - Python implementation.

Pure-Python encoder/decoder for the LwPKT wire protocol, mirroring the reference
C implementation at https://github.com/MaJerle/lwpkt.
"""
import enum
import queue
import struct
import time
from typing import Final

START_BYTE: Final[int] = 0xAA
STOP_BYTE: Final[int] = 0x55


class LwPKTError(Exception):
    """Base class for all lwpkt packet-decoding errors."""


class LwPKTLengthError(LwPKTError):
    """Decoded 'len' field exceeds max_data_len, or its encoding used too many bytes."""


class LwPKTFieldOverflowError(LwPKTError):
    """A from/to/cmd/flags field used more continuation bytes than it ever legitimately could."""


class LwPKTCRCError(LwPKTError):
    """Computed CRC does not match the CRC received on the wire."""


class LwPKTStopByteError(LwPKTError):
    """Byte at the expected STOP position was not the stop marker."""


class LwPKT(object):
    """LwPKT protocol instance: encodes outgoing packets and decodes incoming bytes."""

    class Packet(object):
        """A single in-progress or fully decoded LwPKT packet."""

        class State(enum.Enum):
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

        def __init__(self) -> None:
            self.state: LwPKT.Packet.State = LwPKT.Packet.State.START
            self.len: int = 0
            self.data: bytearray = bytearray()
            self.pkt_from: int = 0
            self.pkt_to: int = 0
            self.flags: int = 0
            self.cmd: int = 0
            self.crc: int = 0
            self.crc_recv: int = 0
            self.index: int = 0

        def _go_to_state(self, state: State) -> None:
            """Reset the per-field index and move to a new parser state."""
            self.index = 0
            self.state = state

    # Continuation-byte cap for from/to/cmd/flags: enough to cover the full
    # practical range (~34 bits) these uint32-ish fields can represent.
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
        self.max_data_len: int = 0  # 0 means unlimited, or else caps the max accepted data length

        # Contains raw RX bytes, each write_rx_data() call enqueues one block
        self.rx_data: queue.Queue = queue.Queue()

        # Contains receive object, reset for each new packet
        self.rx: LwPKT.Packet = LwPKT.Packet()

        # Contains queue of valid receive packets
        self.rx_packets: queue.Queue = queue.Queue()

        # Bytes left over from a block that raised mid-parse, resumed on the next rx_process() call
        self._pending: list[int] | None = None

        # RX throughput debug counters
        self.total_rx_bytes: int = 0
        self.time_last_rx_bytes_reported: float = 0.0

    def generate_packet(self, data: bytes | None, cmd: int = 0, addr_to: int = 0,
                         flags: int = 0) -> bytes:
        """Encode a single outgoing packet.

        Args:
            data: Payload bytes to include, or None/empty bytes for no payload.
            cmd: Command field value (only used if `opt_cmd` is enabled).
            addr_to: Destination address (only used if `opt_addr` is enabled).
            flags: Custom flags value (only used if `opt_flags` is enabled).

        Returns:
            The fully encoded packet, including start byte, CRC, and stop byte.
        """
        data_out = bytearray()

        # Start byte goes here
        data_out.append(START_BYTE)

        # Add address
        if self.opt_addr:
            if self.opt_addr_ext:
                data_out += self._varint_encode(self.our_addr)
                data_out += self._varint_encode(addr_to)
            else:
                data_out.append(self.our_addr & 0xFF)
                data_out.append(addr_to & 0xFF)

        # Add custom flags
        if self.opt_flags:
            data_out += self._varint_encode(flags)

        # Add command
        if self.opt_cmd:
            if self.opt_cmd_ext:
                data_out += self._varint_encode(cmd)
            else:
                data_out.append(cmd & 0xFF)

        # Add data length, then actual data
        datalen = len(data) if data else 0
        data_out += self._varint_encode(datalen)
        if datalen > 0:
            data_out += data

        # Calc CRC of all data (except start byte)
        if self.opt_crc:
            crc = 0xFFFFFFFF if self.opt_crc32 else 0
            for val in data_out[1:]:
                crc = self._crc_in(crc, val)
            if self.opt_crc32:
                crc = crc ^ 0xFFFFFFFF
                data_out += struct.pack('<I', crc)
            else:
                data_out.append(crc)

        # Packet ends here
        data_out.append(STOP_BYTE)
        return data_out

    def write_rx_data(self, data: bytes) -> None:
        """Enqueue raw bytes received from the transport for later processing.

        Args:
            data: Raw bytes as received (e.g. from a serial port read).
        """
        self.rx_data.put_nowait(data)

    def rx_process(self) -> bool:
        """Process previously-written RX data.

        Data must be previously written using write_rx_data(). Consumes all currently
        queued blocks (plus any carried over from a previous call that raised mid-parse),
        running each byte through the receive state machine.

        Returns:
            True if at least one valid packet was queued during this call, False otherwise.
            Use rx_get_packet() to retrieve queued packets.

        Raises:
            LwPKTLengthError: the 'len' field is too large or used too many encoded bytes.
            LwPKTFieldOverflowError: a from/to/cmd/flags field used too many continuation bytes.
            LwPKTCRCError: the computed CRC does not match the CRC received on the wire.
            LwPKTStopByteError: the byte at the expected STOP position was wrong.
            LwPKTError: an internal parser state was reached that should be unreachable.

            In every case, the packet in progress is discarded and the parser is reset to
            START before the exception is raised, so a subsequent rx_process() call can
            resync on the next start byte - no bytes already queued in self.rx_packets from
            earlier in the same call are lost, and any unconsumed bytes from the same block
            are retried on the next call.
        """
        current_time = time.time()
        ret = False

        while True:
            if self._pending is not None:
                block, self._pending = self._pending, None
            elif not self.rx_data.empty():
                block = self.rx_data.get_nowait()
            else:
                break

            self.total_rx_bytes += len(block)
            if current_time - self.time_last_rx_bytes_reported >= 5.0:
                self.time_last_rx_bytes_reported = current_time
                # print('LWPKT debug: Total RX bytes to this moment:', self.total_rx_bytes)

            it = iter(block)
            try:
                for ch in it:
                    match self.rx.state:
                        case LwPKT.Packet.State.START:
                            if ch == START_BYTE:
                                self.rx = LwPKT.Packet()
                                self.rx.crc = 0xFFFFFFFF if self.opt_crc32 else 0
                                self._rx_go_to_next_state()

                        case LwPKT.Packet.State.FROM:
                            self.rx.crc = self._crc_in(self.rx.crc, ch)
                            if self.opt_addr_ext:
                                self.rx.pkt_from |= (ch & 0x7F) << (7 * self.rx.index)
                                self.rx.index += 1
                                if (ch & 0x80) == 0:
                                    self._rx_go_to_next_state()
                                elif self.rx.index >= self._FIELD_MAX_ENC_BYTES:
                                    self.rx._go_to_state(LwPKT.Packet.State.START)
                                    raise LwPKTFieldOverflowError('"from" field used too many continuation bytes')
                            else:
                                self.rx.pkt_from = ch
                                self._rx_go_to_next_state()

                        case LwPKT.Packet.State.TO:
                            self.rx.crc = self._crc_in(self.rx.crc, ch)
                            if self.opt_addr_ext:
                                self.rx.pkt_to |= (ch & 0x7F) << (7 * self.rx.index)
                                self.rx.index += 1
                                if (ch & 0x80) == 0:
                                    self._rx_go_to_next_state()
                                elif self.rx.index >= self._FIELD_MAX_ENC_BYTES:
                                    self.rx._go_to_state(LwPKT.Packet.State.START)
                                    raise LwPKTFieldOverflowError('"to" field used too many continuation bytes')
                            else:
                                self.rx.pkt_to = ch
                                self._rx_go_to_next_state()

                        case LwPKT.Packet.State.FLAGS:
                            self.rx.crc = self._crc_in(self.rx.crc, ch)
                            self.rx.flags |= (ch & 0x7F) << (7 * self.rx.index)
                            self.rx.index += 1
                            if (ch & 0x80) == 0:
                                self._rx_go_to_next_state()
                            elif self.rx.index >= self._FIELD_MAX_ENC_BYTES:
                                self.rx._go_to_state(LwPKT.Packet.State.START)
                                raise LwPKTFieldOverflowError('"flags" field used too many continuation bytes')

                        case LwPKT.Packet.State.CMD:
                            self.rx.crc = self._crc_in(self.rx.crc, ch)
                            if self.opt_cmd_ext:
                                self.rx.cmd |= (ch & 0x7F) << (7 * self.rx.index)
                                self.rx.index += 1
                                if (ch & 0x80) == 0:
                                    self._rx_go_to_next_state()
                                elif self.rx.index >= self._FIELD_MAX_ENC_BYTES:
                                    self.rx._go_to_state(LwPKT.Packet.State.START)
                                    raise LwPKTFieldOverflowError('"cmd" field used too many continuation bytes')
                            else:
                                self.rx.cmd = ch
                                self._rx_go_to_next_state()

                        case LwPKT.Packet.State.LEN:
                            self.rx.crc = self._crc_in(self.rx.crc, ch)
                            self.rx.len |= (ch & 0x7F) << (7 * self.rx.index)
                            self.rx.index += 1

                            if (ch & 0x80) == 0:
                                if self.max_data_len > 0 and self.rx.len > self.max_data_len:
                                    self.rx._go_to_state(LwPKT.Packet.State.START)
                                    raise LwPKTLengthError(
                                        f'Decoded length {self.rx.len} exceeds '
                                        f'max_data_len={self.max_data_len}')
                                self._rx_go_to_next_state()
                            else:
                                max_bytes = (self._max_enc_bytes(self.max_data_len)
                                             if self.max_data_len > 0
                                             else self._FIELD_MAX_ENC_BYTES)
                                if self.rx.index >= max_bytes:
                                    self.rx._go_to_state(LwPKT.Packet.State.START)
                                    raise LwPKTLengthError(
                                        f'"len" field used too many continuation bytes '
                                        f'(> {max_bytes})')

                        case LwPKT.Packet.State.DATA:
                            self.rx.crc = self._crc_in(self.rx.crc, ch)
                            self.rx.data.append(ch)

                            # Try to read more directly from the same block, without
                            # re-entering the outer dispatch loop for every byte
                            while len(self.rx.data) < self.rx.len:
                                nxt = next(it, None)
                                if nxt is None:
                                    break
                                self.rx.crc = self._crc_in(self.rx.crc, nxt)
                                self.rx.data.append(nxt)

                            if len(self.rx.data) == self.rx.len:
                                self._rx_go_to_next_state()

                        case LwPKT.Packet.State.CRC:
                            crc_len = 4 if self.opt_crc32 else 1
                            if self.rx.index < crc_len:
                                self.rx.crc_recv |= ch << (8 * self.rx.index)
                                self.rx.index += 1

                            if self.rx.index == crc_len:
                                if self.opt_crc32:
                                    self.rx.crc ^= 0xFFFFFFFF
                                self._rx_go_to_next_state()

                        case LwPKT.Packet.State.STOP:
                            if ch != STOP_BYTE:
                                self.rx._go_to_state(LwPKT.Packet.State.START)
                                raise LwPKTStopByteError(
                                    f'Expected stop byte 0x{STOP_BYTE:02X}, got 0x{ch:02X}')
                            if self.opt_crc and self.rx.crc != self.rx.crc_recv:
                                self.rx._go_to_state(LwPKT.Packet.State.START)
                                raise LwPKTCRCError(
                                    f'CRC mismatch: expected 0x{self.rx.crc:X}, '
                                    f'got 0x{self.rx.crc_recv:X}')

                            self.rx_packets.put_nowait(self.rx)
                            ret = True
                            self.rx._go_to_state(LwPKT.Packet.State.START)

                        # Handle default situation - should be unreachable
                        case _:
                            self.rx._go_to_state(LwPKT.Packet.State.START)
                            raise LwPKTError(
                                f'Unexpected internal parser state: {self.rx.state!r}')
            except LwPKTError:
                # Preserve remaining data
                self._pending = list(it)
                raise

        return ret

    def rx_get_packet(self) -> Packet | bool:
        """Retrieve the next fully decoded packet, if any.

        Returns:
            The next `Packet` ready to be read, or `False` if none is available.
        """
        if not self.rx_packets.empty():
            return self.rx_packets.get_nowait()
        return False

    def _varint_encode(self, num: int) -> bytearray:
        """Encode an integer as a 7-bit variable-length byte sequence, LSB group first."""
        out = bytearray()
        while True:
            val = num & 0x7F
            if num > 0x7F:
                val = val | 0x80
            num = num >> 7
            out.append(val)
            if num == 0:
                break
        return out

    def _crc_in(self, crc_old_val: int, new_byt: int) -> int:
        """Fold one byte into a running bit-serial CRC (8-bit or 32-bit, per opt_crc32)."""
        for _ in range(8):
            m = (crc_old_val ^ new_byt) & 0x01
            crc_old_val = crc_old_val >> 1
            if m:
                crc_old_val = crc_old_val ^ (0xEDB88320 if self.opt_crc32 else 0x8C)
            new_byt = new_byt >> 1
        return crc_old_val

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
        new_state = LwPKT.Packet.State.END
        match self.rx.state:
            case LwPKT.Packet.State.START:
                if self.opt_addr: new_state = LwPKT.Packet.State.FROM
                elif self.opt_flags: new_state = LwPKT.Packet.State.FLAGS
                elif self.opt_cmd: new_state = LwPKT.Packet.State.CMD
                else: new_state = LwPKT.Packet.State.LEN
            case LwPKT.Packet.State.TO:
                if self.opt_flags: new_state = LwPKT.Packet.State.FLAGS
                elif self.opt_cmd: new_state = LwPKT.Packet.State.CMD
                else: new_state = LwPKT.Packet.State.LEN
            case LwPKT.Packet.State.FLAGS:
                if self.opt_cmd: new_state = LwPKT.Packet.State.CMD
                else: new_state = LwPKT.Packet.State.LEN
            case LwPKT.Packet.State.CMD:
                new_state = LwPKT.Packet.State.LEN
            case LwPKT.Packet.State.LEN:
                if self.rx.len > 0: new_state = LwPKT.Packet.State.DATA
                elif self.opt_crc: new_state = LwPKT.Packet.State.CRC
                else: new_state = LwPKT.Packet.State.STOP
            case LwPKT.Packet.State.DATA:
                if self.opt_crc: new_state = LwPKT.Packet.State.CRC
                else: new_state = LwPKT.Packet.State.STOP
            case LwPKT.Packet.State.CRC:
                new_state = LwPKT.Packet.State.STOP
            case LwPKT.Packet.State.FROM:
                # There is no other way to go than into TO state
                new_state = LwPKT.Packet.State.TO
            case LwPKT.Packet.State.STOP:
                new_state = LwPKT.Packet.State.START
            case _:
                new_state = LwPKT.Packet.State.START
        if new_state != LwPKT.Packet.State.END:
            self.rx._go_to_state(new_state)
