#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WEST="${WEST:-$HOME/ble-dongle-build/.venv/bin/west}"
ZEPHYR_BASE="${ZEPHYR_BASE:-$HOME/ble-dongle-build/zephyrproject/zephyr}"

BOARD="heltec_t114_v2/nrf52840"
APP="$ZEPHYR_BASE/samples/bluetooth/hci_usb"
BUILD_DIR="$REPO_ROOT/build/platypus-heltec-t114-hci-usb-base"

echo "[*] Building Platypus base-board UF2"
echo "[*] Board: $BOARD"
echo "[*] App: $APP"
echo "[*] Build dir: $BUILD_DIR"

cd "$ZEPHYR_BASE"

"$WEST" build -p always \
  -b "$BOARD" \
  "$APP" \
  -d "$BUILD_DIR" \
  -- -DCONFIG_BUILD_OUTPUT_UF2=y

ls -lh "$BUILD_DIR/zephyr/zephyr.uf2"
