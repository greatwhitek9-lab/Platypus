#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WEST="${WEST:-$HOME/ble-dongle-build/.venv/bin/west}"
ZEPHYR_BASE="${ZEPHYR_BASE:-$HOME/ble-dongle-build/zephyrproject/zephyr}"

BOARD="heltec_t114_v2/nrf52840/uf2"
APP="$ZEPHYR_BASE/samples/subsys/usb/cdc_acm"
BUILD_DIR="$REPO_ROOT/build/platypus-usb-cdc-test"

echo "[*] Building USB CDC test for Heltec T114"
echo "[*] Board: $BOARD"
echo "[*] App: $APP"

cd "$ZEPHYR_BASE"

"$WEST" build -p always \
  -b "$BOARD" \
  "$APP" \
  -d "$BUILD_DIR"

ls -lh "$BUILD_DIR/zephyr/zephyr.uf2"
