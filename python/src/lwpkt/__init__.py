"""Lightweight packet protocol (LwPKT) - Python implementation.

Pure-Python encoder/decoder for the LwPKT wire protocol, mirroring the reference
C implementation at https://github.com/MaJerle/lwpkt.
"""
from .lwpkt import (
    LwPKT,
    LwPKTCRCError,
    LwPKTError,
    LwPKTFieldOverflowError,
    LwPKTLengthError,
    LwPKTStopByteError,
    START_BYTE,
    STOP_BYTE,
)

__all__ = [
    "LwPKT",
    "LwPKTError",
    "LwPKTLengthError",
    "LwPKTFieldOverflowError",
    "LwPKTCRCError",
    "LwPKTStopByteError",
    "START_BYTE",
    "STOP_BYTE",
]
