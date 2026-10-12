#!/usr/bin/env python3
"""
Shared wire-protocol reader for the Platypus Sniffer dongle.

Mirrors firmware/platypus-sniffer/src/proto_framer.c:

    0xAA 0x55 | len:u16 LE | type:u8 | payload[len-1] | crc16:u16 LE

Used by both platypus_sniff.py (standalone capture -> pcap) and
tools/extcap/platypus_extcap.py (live capture inside Wireshark).
"""
from __future__ import annotations

import dataclasses
import struct
from typing import Iterator, Optional

MAGIC0 = 0xAA
MAGIC1 = 0x55

MSG_CAPTURE = 0x01
MSG_PING = 0x02
MSG_PONG = 0x03
MSG_SET_MODE = 0x04
MSG_VERSION = 0x05

_CAPTURE_FIXED_FMT = "<IBbB I Q B"  # access_addr, chan, rssi, crc_ok, crc, ts_us, length
_CAPTURE_FIXED_LEN = struct.calcsize(_CAPTURE_FIXED_FMT)


@dataclasses.dataclass
class CapturedPacket:
    access_address: int
    channel: int
    rssi_dbm: int
    crc_ok: bool
    crc: int
    timestamp_us: int
    pdu: bytes


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def parse_capture_payload(payload: bytes) -> CapturedPacket:
    access_address, channel, rssi, crc_ok, crc, ts_us, length = struct.unpack(
        _CAPTURE_FIXED_FMT, payload[:_CAPTURE_FIXED_LEN]
    )
    pdu = payload[_CAPTURE_FIXED_LEN : _CAPTURE_FIXED_LEN + length]
    return CapturedPacket(
        access_address=access_address,
        channel=channel,
        rssi_dbm=rssi,
        crc_ok=bool(crc_ok),
        crc=crc,
        timestamp_us=ts_us,
        pdu=pdu,
    )


class FrameReader:
    """Feed raw serial bytes in; get decoded (type, payload) frames out."""

    def __init__(self) -> None:
        self._buf = bytearray()

    def feed(self, data: bytes) -> Iterator[tuple[int, bytes]]:
        self._buf.extend(data)
        while True:
            frame = self._try_extract_one()
            if frame is None:
                return
            yield frame

    def _try_extract_one(self) -> Optional[tuple[int, bytes]]:
        buf = self._buf
        # Resync on the two magic bytes.
        while len(buf) >= 2 and not (buf[0] == MAGIC0 and buf[1] == MAGIC1):
            del buf[0]
        if len(buf) < 5:
            return None

        length = buf[2] | (buf[3] << 8)  # covers type + payload, not crc
        total_needed = 2 + 2 + length + 2
        if len(buf) < total_needed:
            return None

        type_and_payload = bytes(buf[4 : 4 + length])
        crc_rx = buf[4 + length] | (buf[4 + length + 1] << 8)
        crc_calc = crc16_ccitt_false(type_and_payload)

        del buf[:total_needed]

        if crc_rx != crc_calc:
            # Drop a malformed frame rather than desyncing forever.
            return self._try_extract_one()

        msg_type = type_and_payload[0]
        payload = type_and_payload[1:]
        return (msg_type, payload)
