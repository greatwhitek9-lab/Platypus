#!/usr/bin/env bash
set -euo pipefail

# Verify that Naughty Platypus is running as USB CDC ACM lab firmware.

PORT="${1:-${SERIAL_PORT:-}}"
HOST_TOOL="${HOST_TOOL:-tools/naughty_platypus_host.py}"

echo "=== USB devices ==="
lsusb | grep -Ei "zephyr|nordic|naughty|platypus|2fe3|239a" || true

echo
echo "=== Serial devices ==="
ls -l /dev/ttyACM* 2>/dev/null || echo "No ttyACM devices"

if [[ -z "$PORT" ]]; then
    for dev in /dev/ttyACM*; do
        [[ -e "$dev" ]] || continue
        PORT="$dev"
        break
    done
fi

[[ -n "$PORT" && -e "$PORT" ]] || {
    echo "[x] No serial port found. Plug the Heltec in normally after flashing." >&2
    exit 1
}

echo
echo "[+] Candidate Naughty Platypus port: $PORT"

if [[ -f "$HOST_TOOL" ]]; then
    echo
    echo "[*] Running 10-second collector smoke test"
    mkdir -p captures
    python3 "$HOST_TOOL" --port "$PORT" --duration 10 --csv captures/smoke.csv --jsonl captures/smoke.jsonl
else
    echo "[!] Host tool not found: $HOST_TOOL"
    echo "    Manual console: screen $PORT 115200"
fi
