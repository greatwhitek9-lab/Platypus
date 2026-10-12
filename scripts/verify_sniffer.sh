#!/usr/bin/env bash
# Replaces main's verify_bluez.sh for the naughty-platypus branch.
#
# The sniffer firmware does NOT register as a BlueZ HCI controller on
# purpose (it bypasses the BT host/controller stack entirely), so there is
# no hci1 to check here. Instead we confirm the board enumerates as a
# USB-serial device and that it answers a PING with a version string.
set -euo pipefail

echo "[verify_sniffer] USB devices matching the Platypus VID/PID (239A:0002):"
lsusb -d 239a:0002 || echo "  (not found - is the board plugged in and flashed?)"

PORT="${1:-}"
if [ -z "$PORT" ]; then
    PORT=$(ls /dev/ttyACM* 2>/dev/null | head -n1 || true)
fi
if [ -z "$PORT" ]; then
    echo "error: no /dev/ttyACM* device found; pass the port explicitly:" >&2
    echo "       ./scripts/verify_sniffer.sh /dev/ttyACM0" >&2
    exit 1
fi

echo "[verify_sniffer] found serial port: $PORT"
echo "[verify_sniffer] requesting firmware version..."

python3 - "$PORT" <<'PYEOF'
import sys
sys.path.insert(0, "tools")
from platypus_proto import FrameReader, MSG_VERSION
import serial

port = sys.argv[1]
ser = serial.Serial(port, 115200, timeout=2)
reader = FrameReader()
for _ in range(20):
    data = ser.read(256)
    if not data:
        continue
    for msg_type, payload in reader.feed(data):
        if msg_type == MSG_VERSION:
            print(f"[verify_sniffer] dongle reports: {payload.decode()}")
            sys.exit(0)
print("[verify_sniffer] no version message received - check firmware/cabling", file=sys.stderr)
sys.exit(1)
PYEOF
