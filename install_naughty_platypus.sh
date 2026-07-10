#!/usr/bin/env bash
set -euo pipefail

# Naughty Platypus one-shot installer
# Target: Heltec T114 / HT-n5262 / nRF52840
#
# This script prepares the Linux host, builds the Naughty Platypus UF2, flashes
# the Heltec T114, verifies the runtime serial device, and can launch the host
# collector.
#
# Board stitching intentionally matches the working HT-n5262 settings:
#   CONFIG_USE_DT_CODE_PARTITION=n
#   CONFIG_FLASH_LOAD_OFFSET=0x1000
#   CONFIG_FLASH_LOAD_SIZE=0xdf000
#   UF2 family patched to 0x239a0071
#
# Safety scope:
#   This installer builds the passive lab/survey firmware scaffold. Restricted
#   tools remain default-n / disabled / stub-only unless a private lab developer
#   separately replaces them outside this public one-shot path.

APP_NAME="Naughty Platypus"
BOARD_NAME="HT-n5262"
BOARD_TARGET="${BOARD_TARGET:-heltec_t114_v2/nrf52840/uf2}"

UF2_FAMILY="${UF2_FAMILY:-0x239a0071}"
FLASH_OFFSET="${FLASH_OFFSET:-0x1000}"
FLASH_SIZE="${FLASH_SIZE:-0xdf000}"

USB_ID_BOOTLOADER="${USB_ID_BOOTLOADER:-239a:0071}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

ZEPHYR_BASE="${ZEPHYR_BASE:-$HOME/ble-dongle-build/zephyrproject/zephyr}"
WEST="${WEST:-$HOME/ble-dongle-build/.venv/bin/west}"

APP_DIR="${APP_DIR:-$REPO_ROOT/firmware/naughty-platypus}"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build/naughty-platypus-offset1000}"
RELEASE_DIR="${RELEASE_DIR:-$REPO_ROOT/releases}"
RELEASE_UF2="${RELEASE_UF2:-$RELEASE_DIR/naughty-platypus-HT-n5262-offset1000.uf2}"
MOUNT_DIR="${MOUNT_DIR:-/mnt/t114}"

HOST_TOOL="${HOST_TOOL:-$REPO_ROOT/tools/naughty_platypus_host.py}"
CAPTURE_DIR="${CAPTURE_DIR:-$REPO_ROOT/captures}"
VERIFY_SECONDS="${VERIFY_SECONDS:-20}"
SERIAL_PORT="${SERIAL_PORT:-}"

ASSUME_YES=0
INSTALL_DEPS=0
HOST_ONLY=0
BUILD_ONLY=0
FLASH_ONLY=0
SKIP_BUILD=0
NO_FLASH=0
NO_VERIFY=0
RUN_HOST=0
NO_RUN_HOST=0
SETUP_ZEPHYR=0

usage() {
    cat <<EOF
$APP_NAME one-shot installer

Usage:
  ./install_naughty_platypus.sh [options]

Common:
  ./install_naughty_platypus.sh --install-deps
  ./install_naughty_platypus.sh
  ./install_naughty_platypus.sh --skip-build
  ./install_naughty_platypus.sh --run-host

Options:
  --yes              Assume yes for prompts where possible
  --install-deps     Install common Kali/Debian host dependencies with apt
  --setup-zephyr     Create a Zephyr workspace if WEST/ZEPHYR_BASE are missing
  --host-only        Only install/check host-side tools; do not build or flash
  --build-only       Build and patch UF2 only; do not flash
  --flash-only       Skip build and flash existing RELEASE_UF2
  --skip-build       Same as --flash-only
  --no-flash         Build but do not flash
  --no-verify        Skip serial-device verification after flashing
  --run-host         After verification, run a short host survey capture
  --no-run-host      Do not offer/launch the host collector
  --port PATH        Serial port for host collector, e.g. /dev/ttyACM0
  -h, --help         Show this help

Environment overrides:
  ZEPHYR_BASE        Default: $HOME/ble-dongle-build/zephyrproject/zephyr
  WEST               Default: $HOME/ble-dongle-build/.venv/bin/west
  BOARD_TARGET       Default: heltec_t114_v2/nrf52840/uf2
  RELEASE_UF2        Default: ./releases/naughty-platypus-HT-n5262-offset1000.uf2
  MOUNT_DIR          Default: /mnt/t114
  SERIAL_PORT        Optional serial port override
  VERIFY_SECONDS     Default: 20
  CAPTURE_DIR        Default: ./captures

Expected board-specific UF2 metadata:
  Address min: 0x00001000
  Families: ['0x239a0071']
EOF
}

log() { echo "[*] $*"; }
ok() { echo "[+] $*"; }
warn() { echo "[!] $*" >&2; }
fail() { echo "[x] $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1; }

confirm() {
    local prompt="$1"
    if [[ "$ASSUME_YES" -eq 1 ]]; then
        return 0
    fi
    read -r -p "$prompt [y/N] " ans
    [[ "$ans" =~ ^[Yy]$ ]]
}

for ((i=1; i<=$#; i++)); do
    arg="${!i}"
    case "$arg" in
        --yes) ASSUME_YES=1 ;;
        --install-deps) INSTALL_DEPS=1 ;;
        --setup-zephyr) SETUP_ZEPHYR=1 ;;
        --host-only) HOST_ONLY=1 ;;
        --build-only) BUILD_ONLY=1; NO_FLASH=1 ;;
        --flash-only) FLASH_ONLY=1; SKIP_BUILD=1 ;;
        --skip-build) SKIP_BUILD=1 ;;
        --no-flash) NO_FLASH=1 ;;
        --no-verify) NO_VERIFY=1 ;;
        --run-host) RUN_HOST=1 ;;
        --no-run-host) NO_RUN_HOST=1 ;;
        --port)
            ((i++)) || fail "--port requires a path"
            SERIAL_PORT="${!i}"
            ;;
        -h|--help) usage; exit 0 ;;
        *) fail "Unknown option: $arg" ;;
    esac
done

banner() {
    cat <<EOF

============================================================
  Naughty Platypus One-Shot Installer
============================================================
  Board:      Heltec T114 / HT-n5262 / nRF52840
  Firmware:   USB CDC ACM passive BLE lab/survey firmware
  Host tool:  tools/naughty_platypus_host.py
  Fixes:      app offset 0x1000 + size 0xdf000 + UF2 family 0x239a0071
============================================================

EOF
}

install_deps() {
    if [[ "$INSTALL_DEPS" -ne 1 ]]; then
        return 0
    fi

    if ! need_cmd apt; then
        warn "apt not found; skipping Debian/Kali dependency install."
        return 0
    fi

    log "Installing common host/build dependencies"
    sudo apt update
    sudo apt install -y \
        git python3 python3-venv python3-pip python3-serial \
        cmake ninja-build gperf ccache dfu-util device-tree-compiler \
        wget curl xz-utils file make gcc g++ gcc-multilib libsdl2-dev libmagic1 \
        usbutils util-linux screen unzip

    ok "Host dependencies installed."
}

setup_zephyr_if_requested() {
    if [[ "$SETUP_ZEPHYR" -ne 1 ]]; then
        return 0
    fi

    if [[ -x "$WEST" && -d "$ZEPHYR_BASE" ]]; then
        ok "Existing Zephyr workspace found:"
        echo "    WEST=$WEST"
        echo "    ZEPHYR_BASE=$ZEPHYR_BASE"
        return 0
    fi

    local base="$HOME/ble-dongle-build"
    local venv="$base/.venv"
    local project="$base/zephyrproject"

    log "Setting up Zephyr workspace under $base"
    mkdir -p "$base"

    if [[ ! -d "$venv" ]]; then
        python3 -m venv "$venv"
    fi

    # shellcheck disable=SC1091
    source "$venv/bin/activate"

    python3 -m pip install --upgrade pip wheel
    python3 -m pip install west

    if [[ ! -d "$project/zephyr" ]]; then
        log "Initializing Zephyr workspace"
        west init "$project"
        cd "$project"
        west update
    else
        log "Updating existing Zephyr workspace"
        cd "$project"
        west update
    fi

    log "Installing Zephyr Python requirements"
    python3 -m pip install -r "$project/zephyr/scripts/requirements.txt"

    WEST="$venv/bin/west"
    ZEPHYR_BASE="$project/zephyr"

    ok "Zephyr workspace ready:"
    echo "    WEST=$WEST"
    echo "    ZEPHYR_BASE=$ZEPHYR_BASE"
}

check_host_files() {
    [[ -d "$APP_DIR" ]] || fail "Firmware app not found: $APP_DIR. Apply the Naughty Platypus patch first."
    [[ -f "$HOST_TOOL" ]] || fail "Host collector not found: $HOST_TOOL. Apply the Naughty Platypus patch first."
    [[ -f "$REPO_ROOT/tools/inspect_uf2.py" ]] || fail "Missing tools/inspect_uf2.py"
    [[ -f "$REPO_ROOT/tools/patch_uf2_family.py" ]] || fail "Missing tools/patch_uf2_family.py"
}

check_host_runtime() {
    log "Checking host-side runtime"
    python3 - <<'PY'
try:
    import serial
    print("[+] Python serial module available")
except Exception as e:
    print("[!] Python serial module unavailable:", e)
    raise SystemExit(1)
PY
    ok "Host-side runtime checks passed."
}

build_firmware() {
    [[ -x "$WEST" ]] || fail "west not found or not executable: $WEST. Run with --setup-zephyr or set WEST=/path/to/west."
    [[ -d "$ZEPHYR_BASE" ]] || fail "Zephyr base not found: $ZEPHYR_BASE. Run with --setup-zephyr or set ZEPHYR_BASE=/path/to/zephyr."
    check_host_files

    mkdir -p "$RELEASE_DIR"

    if [[ -x "$REPO_ROOT/scripts/build_naughty_platypus.sh" ]]; then
        log "Using scripts/build_naughty_platypus.sh"
        ZEPHYR_BASE="$ZEPHYR_BASE" WEST="$WEST" BOARD_TARGET="$BOARD_TARGET" \
        FLASH_OFFSET="$FLASH_OFFSET" FLASH_SIZE="$FLASH_SIZE" UF2_FAMILY="$UF2_FAMILY" \
        RELEASE_UF2="$RELEASE_UF2" "$REPO_ROOT/scripts/build_naughty_platypus.sh"
        return 0
    fi

    log "Building firmware with inline one-shot build path"
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
        "$APP_DIR" \
        -d "$BUILD_DIR" \
        -- \
        -DCONFIG_USE_DT_CODE_PARTITION=n \
        -DCONFIG_FLASH_LOAD_OFFSET="$FLASH_OFFSET" \
        -DCONFIG_FLASH_LOAD_SIZE="$FLASH_SIZE" \
    -DKCONFIG_ERROR_ON_WARNINGS=OFF

    local raw_uf2="$BUILD_DIR/zephyr/zephyr.uf2"
    [[ -f "$raw_uf2" ]] || fail "Build completed but UF2 not found: $raw_uf2"

    log "Raw UF2 metadata"
    python3 "$REPO_ROOT/tools/inspect_uf2.py" "$raw_uf2"

    log "Patching UF2 family to $UF2_FAMILY"
    python3 "$REPO_ROOT/tools/patch_uf2_family.py" "$raw_uf2" "$RELEASE_UF2" "$UF2_FAMILY"

    log "Patched UF2 metadata"
    python3 "$REPO_ROOT/tools/inspect_uf2.py" "$RELEASE_UF2"

    ok "Release UF2 ready: $RELEASE_UF2"
}

find_uf2_device() {
    lsblk -pnro NAME,LABEL,FSTYPE 2>/dev/null | awk '$2=="HT-n5262" && $3=="vfat" {print $1; exit}'
}

flash_firmware() {
    [[ -f "$RELEASE_UF2" ]] || fail "Release UF2 not found: $RELEASE_UF2. Build first or provide RELEASE_UF2=/path/to/file.uf2."

    cat <<EOF

Flash step
----------
Put the Heltec T114 into UF2 bootloader mode now:

  1. Plug in the board.
  2. Double-tap RST.
  3. Wait for the HT-n5262 removable drive to appear.

EOF

    if [[ "$ASSUME_YES" -ne 1 ]]; then
        read -r -p "Press ENTER after the HT-n5262 UF2 drive appears..."
    fi

    log "Waiting for $BOARD_NAME UF2 drive"
    local dev=""
    for _ in $(seq 1 45); do
        dev="$(find_uf2_device || true)"
        [[ -n "$dev" ]] && break
        sleep 1
    done

    [[ -n "$dev" ]] || {
        warn "Could not find HT-n5262 UF2 drive. Current USB devices:"
        lsusb || true
        warn "Current block devices:"
        lsblk -o NAME,LABEL,SIZE,FSTYPE,MOUNTPOINT || true
        fail "Board not found in UF2 mode. Double-tap RST and retry."
    }

    ok "Found UF2 block device: $dev"

    local existing_mount
    existing_mount="$(lsblk -no MOUNTPOINT "$dev" | head -1 || true)"
    local target_mount="$MOUNT_DIR"
    local mounted_by_script=0

    if [[ -n "$existing_mount" ]]; then
        target_mount="$existing_mount"
        log "Using existing mount: $target_mount"
    else
        log "Mounting $dev at $target_mount"
        sudo mkdir -p "$target_mount"
        sudo mount -t vfat "$dev" "$target_mount"
        mounted_by_script=1
    fi

    log "Copying UF2 to board"
    sudo cp -v "$RELEASE_UF2" "$target_mount/"
    sync

    if [[ "$mounted_by_script" -eq 1 ]]; then
        sudo umount "$target_mount" 2>/dev/null || true
    else
        udisksctl unmount -b "$dev" 2>/dev/null || true
    fi

    ok "Flash copy complete."
    echo
    echo "Unplug the Heltec, wait 5 seconds, then plug it back in normally."
    echo "Do not double-tap RST after flashing."
}

find_serial_port() {
    if [[ -n "$SERIAL_PORT" && -e "$SERIAL_PORT" ]]; then
        echo "$SERIAL_PORT"
        return 0
    fi

    # Prefer a recently-created ACM device.
    for dev in /dev/ttyACM*; do
        [[ -e "$dev" ]] || continue
        echo "$dev"
        return 0
    done

    return 1
}

verify_board_runtime() {
    if [[ "$NO_VERIFY" -eq 1 ]]; then
        ok "Verification skipped."
        return 0
    fi

    cat <<EOF

Verification step
-----------------
After normal replug, Naughty Platypus should appear as a USB CDC ACM serial device,
usually /dev/ttyACM0 or /dev/ttyACM1.

EOF

    if [[ "$ASSUME_YES" -ne 1 ]]; then
        read -r -p "Press ENTER after plugging the board back in normally..."
    else
        sleep 5
    fi

    log "Checking USB and serial devices"

    echo
    echo "USB devices:"
    lsusb | grep -Ei "zephyr|nordic|naughty|platypus|2fe3|239a" || true

    echo
    echo "Serial devices:"
    ls -l /dev/ttyACM* 2>/dev/null || true

    local port=""
    for _ in $(seq 1 "$VERIFY_SECONDS"); do
        if port="$(find_serial_port 2>/dev/null)"; then
            SERIAL_PORT="$port"
            ok "Naughty Platypus serial device found: $SERIAL_PORT"
            return 0
        fi
        sleep 1
    done

    warn "No /dev/ttyACM* device found after verification window."
    warn "Check dmesg output:"
    dmesg | tail -60 || true
    fail "Board did not enumerate as the lab serial firmware."
}

run_host_collector() {
    [[ "$NO_RUN_HOST" -eq 1 ]] && return 0

    if [[ "$RUN_HOST" -ne 1 ]]; then
        if ! confirm "Run a short 30-second host survey capture now?"; then
            return 0
        fi
    fi

    [[ -n "$SERIAL_PORT" ]] || SERIAL_PORT="$(find_serial_port)" || fail "No serial port found for host collector."
    [[ -f "$HOST_TOOL" ]] || fail "Host collector not found: $HOST_TOOL"

    mkdir -p "$CAPTURE_DIR"

    local stamp
    stamp="$(date +%Y%m%d-%H%M%S)"
    local jsonl="$CAPTURE_DIR/naughty-platypus-$stamp.jsonl"
    local csv="$CAPTURE_DIR/naughty-platypus-$stamp.csv"

    log "Starting host collector"
    echo "    Port:  $SERIAL_PORT"
    echo "    JSONL: $jsonl"
    echo "    CSV:   $csv"

    python3 "$HOST_TOOL" \
        --port "$SERIAL_PORT" \
        --duration 30 \
        --jsonl "$jsonl" \
        --csv "$csv"

    ok "Host survey capture complete."
}

main() {
    banner
    install_deps
    setup_zephyr_if_requested
    check_host_files
    check_host_runtime

    if [[ "$HOST_ONLY" -eq 1 ]]; then
        ok "Host-only setup complete."
        exit 0
    fi

    if [[ "$SKIP_BUILD" -eq 1 || "$FLASH_ONLY" -eq 1 ]]; then
        log "Skipping build and using existing UF2: $RELEASE_UF2"
        [[ -f "$RELEASE_UF2" ]] || fail "No release UF2 found. Build first or provide RELEASE_UF2=/path/file.uf2"
    else
        build_firmware
    fi

    if [[ "$BUILD_ONLY" -eq 1 || "$NO_FLASH" -eq 1 ]]; then
        ok "Build-only mode complete."
        exit 0
    fi

    flash_firmware
    verify_board_runtime
    run_host_collector

    ok "$APP_NAME one-shot installation complete."
}

main "$@"
