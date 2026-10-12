#!/usr/bin/env bash
# Build the Platypus Sniffer firmware (replaces the plain BlueZ HCI build
# from main for the naughty-platypus branch).
#
# Reuses the same Zephyr workspace / HT-n5262 flash-offset fix that main's
# scripts/build_platypus.sh already established:
#   CONFIG_USE_DT_CODE_PARTITION=n
#   CONFIG_FLASH_LOAD_OFFSET=0x1000
#   CONFIG_FLASH_LOAD_SIZE=0xdf000
#   UF2 family patched to 0x239a0071
# (those are already baked into firmware/platypus-sniffer/prj.conf and the
# patch step below, not duplicated as manual flags here).
set -euo pipefail

ZEPHYR_BASE="${ZEPHYR_BASE:-$HOME/ble-dongle-build/zephyrproject/zephyr}"
WEST="${WEST:-$HOME/ble-dongle-build/.venv/bin/west}"
BOARD="${BOARD:-heltec_t114_v2/nrf52840/uf2}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_DIR="$REPO_ROOT/firmware/platypus-sniffer"
BUILD_DIR="$REPO_ROOT/build/platypus-sniffer"
OUT_UF2="$REPO_ROOT/releases/platypus-sniffer-HT-n5262-offset1000.uf2"

if [ ! -x "$WEST" ]; then
    echo "error: west not found at $WEST (set WEST=/path/to/west)" >&2
    exit 1
fi
if [ ! -d "$ZEPHYR_BASE" ]; then
    echo "error: Zephyr workspace not found at $ZEPHYR_BASE" >&2
    echo "       (set ZEPHYR_BASE=/path/to/zephyr)" >&2
    exit 1
fi

echo "[build_sniffer] building $APP_DIR for $BOARD"
"$WEST" build -b "$BOARD" -d "$BUILD_DIR" "$APP_DIR" --pristine

mkdir -p "$REPO_ROOT/releases"

echo "[build_sniffer] inspecting raw build output UF2 metadata"
python3 "$REPO_ROOT/tools/inspect_uf2.py" "$BUILD_DIR/zephyr/zephyr.uf2" || true

echo "[build_sniffer] patching UF2 family for HT-n5262"
python3 "$REPO_ROOT/tools/patch_uf2_family.py" \
    "$BUILD_DIR/zephyr/zephyr.uf2" \
    "$OUT_UF2" \
    --family 0x239a0071

echo "[build_sniffer] inspecting patched UF2 metadata"
python3 "$REPO_ROOT/tools/inspect_uf2.py" "$OUT_UF2"

echo "[build_sniffer] done -> $OUT_UF2"
