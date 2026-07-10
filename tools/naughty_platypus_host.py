#!/usr/bin/env python3
"""
Naughty Platypus host collector.

Reads newline-delimited JSON events from the Heltec T114 / nRF52840 firmware,
keeps a live BLE advertisement inventory, and writes optional JSONL/CSV output.

This tool expects the Naughty Platypus CDC ACM firmware, not the standard
Platypus BlueZ USB HCI firmware.
"""

from __future__ import annotations

import argparse
import csv
import json
import signal
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional

try:
    import serial
except ImportError:
    print("Missing dependency: pyserial", file=sys.stderr)
    print("Install with: python3 -m pip install pyserial", file=sys.stderr)
    raise


@dataclass
class DeviceRecord:
    addr: str
    first_seen: float
    last_seen: float
    count: int = 0
    best_rssi: int = -127
    last_rssi: int = -127
    name: str = ""
    phy: str = ""
    flags: Optional[int] = None
    mfg_hex: str = ""
    svc16_hex: str = ""
    ad_len: int = 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Collect BLE survey data from Naughty Platypus.")
    parser.add_argument("--port", required=True, help="Serial port, e.g. /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=0, help="Seconds to run; 0 = until Ctrl+C")
    parser.add_argument("--jsonl", help="Write raw JSON events to this file")
    parser.add_argument("--csv", help="Write final device inventory CSV to this file")
    parser.add_argument("--no-start", action="store_true", help="Do not send np scan_on on connect")
    parser.add_argument("--no-stop", action="store_true", help="Do not send np scan_off on exit")
    parser.add_argument("--print-raw", action="store_true", help="Print every JSON event")
    parser.add_argument("--table-interval", type=float, default=5.0)
    return parser.parse_args()


def send_cmd(ser: serial.Serial, command: str) -> None:
    ser.write((command.strip() + "\\r\\n").encode("utf-8", errors="replace"))
    ser.flush()


def update_record(records: Dict[str, DeviceRecord], event: dict) -> None:
    if event.get("type") != "adv":
        return

    addr = str(event.get("addr", "unknown"))
    now = time.time()
    rssi = int(event.get("rssi", -127))

    rec = records.get(addr)
    if rec is None:
        rec = DeviceRecord(addr=addr, first_seen=now, last_seen=now)
        records[addr] = rec

    rec.last_seen = now
    rec.count += 1
    rec.last_rssi = rssi
    rec.best_rssi = max(rec.best_rssi, rssi)
    rec.phy = str(event.get("phy", rec.phy or ""))
    rec.ad_len = int(event.get("ad_len", rec.ad_len or 0))

    if event.get("name"):
        rec.name = str(event["name"])
    if "flags" in event:
        rec.flags = int(event["flags"])
    if event.get("mfg_hex"):
        rec.mfg_hex = str(event["mfg_hex"])
    if event.get("svc16_hex"):
        rec.svc16_hex = str(event["svc16_hex"])


def print_table(records: Dict[str, DeviceRecord], start: float) -> None:
    rows = sorted(records.values(), key=lambda r: (r.best_rssi, r.count), reverse=True)[:20]
    elapsed = time.time() - start

    print("\\n=== Naughty Platypus BLE survey | %.1fs | devices=%d ===" % (elapsed, len(records)))
    print("%-34s %6s %6s %7s %-8s %-24s %-18s" %
          ("Address", "Last", "Best", "Count", "PHY", "Name", "MFG preview"))
    print("-" * 120)

    for rec in rows:
        print("%-34s %6d %6d %7d %-8s %-24s %-18s" %
              (rec.addr[:34], rec.last_rssi, rec.best_rssi, rec.count,
               rec.phy[:8], rec.name[:24], rec.mfg_hex[:18]))


def write_csv(path: str, records: Dict[str, DeviceRecord]) -> None:
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)

    with p.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "addr", "first_seen_epoch", "last_seen_epoch", "count",
                "last_rssi", "best_rssi", "phy", "name", "flags",
                "ad_len", "mfg_hex", "svc16_hex",
            ],
        )
        writer.writeheader()

        for rec in sorted(records.values(), key=lambda r: r.addr):
            writer.writerow({
                "addr": rec.addr,
                "first_seen_epoch": "%.3f" % rec.first_seen,
                "last_seen_epoch": "%.3f" % rec.last_seen,
                "count": rec.count,
                "last_rssi": rec.last_rssi,
                "best_rssi": rec.best_rssi,
                "phy": rec.phy,
                "name": rec.name,
                "flags": rec.flags if rec.flags is not None else "",
                "ad_len": rec.ad_len,
                "mfg_hex": rec.mfg_hex,
                "svc16_hex": rec.svc16_hex,
            })


def main() -> int:
    args = parse_args()
    records: Dict[str, DeviceRecord] = {}
    stop = False

    def handle_signal(signum, frame):
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    jsonl_file = None
    if args.jsonl:
        p = Path(args.jsonl)
        p.parent.mkdir(parents=True, exist_ok=True)
        jsonl_file = p.open("a", encoding="utf-8")

    start = time.time()
    next_table = start + args.table_interval

    with serial.Serial(args.port, args.baud, timeout=0.5) as ser:
        time.sleep(1.0)

        if not args.no_start:
            send_cmd(ser, "np scan_on")

        while not stop:
            if args.duration and (time.time() - start) >= args.duration:
                break

            line = ser.readline().decode("utf-8", errors="replace").strip()
            if not line:
                if time.time() >= next_table:
                    print_table(records, start)
                    next_table = time.time() + args.table_interval
                continue

            if not line.startswith("{"):
                continue

            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue

            if jsonl_file:
                jsonl_file.write(json.dumps(event, separators=(",", ":")) + "\\n")
                jsonl_file.flush()

            if args.print_raw:
                print(json.dumps(event, indent=2, sort_keys=True))

            update_record(records, event)

            if time.time() >= next_table:
                print_table(records, start)
                next_table = time.time() + args.table_interval

        if not args.no_stop:
            try:
                send_cmd(ser, "np scan_off")
            except Exception:
                pass

    if jsonl_file:
        jsonl_file.close()

    print_table(records, start)

    if args.csv:
        write_csv(args.csv, records)
        print(f"\\nWrote CSV: {args.csv}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
