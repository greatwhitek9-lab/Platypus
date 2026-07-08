#!/usr/bin/env bash
set -euo pipefail

# Build Naughty Platypus for Heltec T114 / HT-n5262 / nRF52840.
#
# This script intentionally preserves the board-specific fixes discovered
# while flashing the Heltec T114:
#
#   CONFIG_USE_DT_CODE_PARTITION=n
#   CONFIG_FLASH_LOAD_OFFSET=0x1000
#   CONFIG_FLASH_LOAD_SIZE=0xdf000
#   UF2 family patched to 0x239a0071
#
# Runtime output mode:
#   USB CDC ACM serial lab instrument
#
# This is separate from the standard Platypus USB HCI firmware.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ZEPHYR_BASE="${ZEPHYR_BASE:-$HOME/ble-dongle-build/zephyrproject/zephyr}"
WEST="${WEST:-$HOME/ble-dongle-build/.venv/bin/west}"
BOARD_TARGET="${BOARD_TARGET:-heltec_t114_v2/nrf52840/uf2}"
APP_DIR="${APP_DIR:-$REPO_ROOT/firmware/naughty-platypus}"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build/naughty-platypus-offset1000}"
RELEASE_DIR="${RELEASE_DIR:-$REPO_ROOT/releases}"
RAW_UF2="$BUILD_DIR/zephyr/zephyr.uf2"
RELEASE_UF2="${RELEASE_UF2:-$RELEASE_DIR/naughty-platypus-HT-n5262-offset1000.uf2}"

FLASH_OFFSET="${FLASH_OFFSET:-0x1000}"
FLASH_SIZE="${FLASH_SIZE:-0xdf000}"
UF2_FAMILY="${UF2_FAMILY:-0x239a0071}"

[[ -x "$WEST" ]] || { echo "west not found or not executable: $WEST" >&2; exit 1; }
[[ -d "$ZEPHYR_BASE" ]] || { echo "ZEPHYR_BASE not found: $ZEPHYR_BASE" >&2; exit 1; }
[[ -d "$APP_DIR" ]] || { echo "app dir not found: $APP_DIR" >&2; exit 1; }

mkdir -p "$RELEASE_DIR"

echo "[*] Building Naughty Platypus"
echo "    Board:       $BOARD_TARGET"
echo "    Zephyr:      $ZEPHYR_BASE"
echo "    West:        $WEST"
echo "    App:         $APP_DIR"
echo "    Build dir:   $BUILD_DIR"
echo "    Output:      $RELEASE_UF2"
echo "    App offset:  $FLASH_OFFSET"
echo "    App size:    $FLASH_SIZE"
echo "    UF2 family:  $UF2_FAMILY"

cd "$ZEPHYR_BASE"

"$WEST" build -p always \
    -b "$BOARD_TARGET" \
    -S cdc-acm-console \
    "$APP_DIR" \
    -d "$BUILD_DIR" \
    -- \
    -DCONFIG_USE_DT_CODE_PARTITION=n \
    -DCONFIG_FLASH_LOAD_OFFSET="$FLASH_OFFSET" \
    -DCONFIG_FLASH_LOAD_SIZE="$FLASH_SIZE"

[[ -f "$RAW_UF2" ]] || { echo "Build completed but UF2 not found: $RAW_UF2" >&2; exit 1; }

echo "[*] Raw UF2 metadata"
python3 "$REPO_ROOT/tools/inspect_uf2.py" "$RAW_UF2"

echo "[*] Patching UF2 family to $UF2_FAMILY"
python3 "$REPO_ROOT/tools/patch_uf2_family.py" "$RAW_UF2" "$RELEASE_UF2" "$UF2_FAMILY"

echo "[*] Patched UF2 metadata"
python3 "$REPO_ROOT/tools/inspect_uf2.py" "$RELEASE_UF2"

echo
echo "[+] Release UF2 ready:"
echo "    $RELEASE_UF2"
echo
echo "Expected metadata:"
echo "    Address min: 0x00001000"
echo "    Families: ['0x239a0071']"
echo
echo "Flash:"
echo "  1. Double-tap RST on the Heltec T114."
echo "  2. Wait for the HT-n5262 UF2 drive."
echo "  3. Copy the UF2 above to that drive."
echo "  4. Unplug/replug normally."
echo
echo "Expected runtime mode:"
echo "  /dev/ttyACM0 or /dev/ttyACM1"
echo
echo "Host collector:"
echo "  python3 tools/naughty_platypus_host.py --port /dev/ttyACM0 --duration 120 --csv captures/survey.csv --jsonl captures/survey.jsonl"
