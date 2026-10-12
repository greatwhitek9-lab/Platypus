#!/usr/bin/env python3
"""
platypus_sniff.py - standalone capture CLI for the Platypus Sniffer dongle.

Reads framed capture records from the dongle's USB-serial port and writes a
PCAP file using LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR (251/256-style pseudo
header), so the output opens directly in Wireshark with its built-in BLE LL
dissector - this is the same output format used by Nordic's nRF Sniffer and
other community BLE sniffers, chosen specifically for Wireshark compatibility
rather than inventing a new one.

Usage:
    python3 platypus_sniff.py --port /dev/ttyACM0 --out capture.pcap
    python3 platypus_sniff.py --port /dev/ttyACM0 --out capture.pcap --live
"""
from __future__ import annotations

import argparse
import struct
import sys
import time

try:
    import serial  # pyserial
except ImportError:
    print("This tool needs pyserial: pip install pyserial", file=sys.stderr)
    raise

from platypus_proto import FrameReader, MSG_CAPTURE, MSG_VERSION, parse_capture_payload

LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR = 256

PCAP_GLOBAL_HDR = struct.pack(
    "<IHHiIII",
    0xA1B2C3D4,  # magic
    2, 4,        # version major/minor
    0,           # thiszone
    0,           # sigfigs
    65535,       # snaplen
    LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR,
)

FLAG_DEWHITENED = 1 << 0
FLAG_SIGPOWER_VALID = 1 << 1
FLAG_REF_AA_VALID = 1 << 4


def build_pcap_record(pkt, out_file) -> None:
    ts_sec = pkt.timestamp_us // 1_000_000
    ts_usec = pkt.timestamp_us % 1_000_000

    pseudo_header = struct.pack(
        "<BbbBIH",
        pkt.channel,
        pkt.rssi_dbm,
        0,  # noise_power: not measured by this firmware
        0,  # access_address_offenses: not tracked
        pkt.access_address,
        FLAG_DEWHITENED | FLAG_SIGPOWER_VALID | FLAG_REF_AA_VALID,
    )

    # LINKTYPE_BLUETOOTH_LE_LL body: AccessAddress + PDU header/payload.
    # Our firmware's "crc_ok" flag is carried via pkt.pdu already stripped of
    # CRC (nRF52840 hardware strips CRC from the RX buffer) - the firmware
    # doesn't currently recover the 3 raw CRC bytes, so we pad zeroes here.
    # See docs/SNIFFER-ARCHITECTURE.md "Known limitations".
    aa_bytes = struct.pack("<I", pkt.access_address)
    body = pseudo_header + aa_bytes + pkt.pdu + b"\x00\x00\x00"

    rec_hdr = struct.pack("<IIII", ts_sec, ts_usec, len(body), len(body))
    out_file.write(rec_hdr)
    out_file.write(body)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", required=True, help="e.g. /dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", required=True, help="output .pcap path")
    ap.add_argument(
        "--live", action="store_true",
        help="flush after every packet (useful for tailing with Wireshark)",
    )
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.2)
    reader = FrameReader()

    with open(args.out, "wb") as f:
        f.write(PCAP_GLOBAL_HDR)
        f.flush()

        count = 0
        print(f"[platypus] capturing from {args.port} -> {args.out}"
              " (Ctrl-C to stop)")
        try:
            while True:
                data = ser.read(4096)
                if not data:
                    continue
                for msg_type, payload in reader.feed(data):
                    if msg_type == MSG_CAPTURE:
                        pkt = parse_capture_payload(payload)
                        build_pcap_record(pkt, f)
                        count += 1
                        if args.live:
                            f.flush()
                        if count % 50 == 0:
                            print(f"[platypus] {count} packets captured",
                                  end="\r", flush=True)
                    elif msg_type == MSG_VERSION:
                        print(f"[platypus] dongle firmware: "
                              f"{payload.decode(errors='replace')}")
        except KeyboardInterrupt:
            pass

    print(f"\n[platypus] done, {count} packets written to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
