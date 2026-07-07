#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

ZEPHYR_WORKSPACE="${ZEPHYR_WORKSPACE:-$HOME/ble-dongle-build/zephyrproject}"
ZEPHYR_BASE="${ZEPHYR_BASE:-$ZEPHYR_WORKSPACE/zephyr}"
WEST="${WEST:-$HOME/ble-dongle-build/.venv/bin/west}"

BOARD="${BOARD:-heltec_t114_v2/nrf52840/uf2}"
APP="${APP:-$REPO_ROOT/firmware/platypus-hci-usb}"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build/platypus-heltec-t114-hci-usb}"

echo "[*] Platypus build"
echo "[*] Heltec T114 BLE USB HCI dongle"
echo "[*] Repo root:   $REPO_ROOT"
echo "[*] Zephyr base: $ZEPHYR_BASE"
echo "[*] West:        $WEST"
echo "[*] Board:       $BOARD"
echo "[*] App:         $APP"
echo "[*] Build dir:   $BUILD_DIR"

if [[ ! -x "$WEST" ]]; then
  echo "[!] west not found at: $WEST"
  exit 1
fi

if [[ ! -d "$ZEPHYR_BASE" ]]; then
  echo "[!] Zephyr base not found at: $ZEPHYR_BASE"
  exit 1
fi

if [[ ! -f "$APP/CMakeLists.txt" ]]; then
  echo "[!] Platypus firmware app not found at: $APP"
  exit 1
fi

cd "$ZEPHYR_BASE"

"$WEST" build -p always \
  -b "$BOARD" \
  "$APP" \
  -d "$BUILD_DIR"

UF2="$BUILD_DIR/zephyr/zephyr.uf2"

if [[ ! -f "$UF2" ]]; then
  echo "[!] Build finished but UF2 was not found at: $UF2"
  exit 1
fi

echo "[+] Platypus build complete:"
ls -lh "$UF2"
