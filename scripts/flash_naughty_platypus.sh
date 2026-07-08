#!/usr/bin/env bash
set -euo pipefail

# Flash Naughty Platypus UF2 to Heltec T114 / HT-n5262.
# Assumes scripts/build_naughty_platypus.sh already produced:
#   releases/naughty-platypus-HT-n5262-offset1000.uf2

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UF2="${1:-$REPO_ROOT/releases/naughty-platypus-HT-n5262-offset1000.uf2}"
MOUNT_DIR="${MOUNT_DIR:-/mnt/t114}"

[[ -f "$UF2" ]] || { echo "UF2 not found: $UF2" >&2; exit 1; }

find_uf2_device() {
    lsblk -pnro NAME,LABEL,FSTYPE 2>/dev/null | awk '$2=="HT-n5262" && $3=="vfat" {print $1; exit}'
}

echo "[*] Put Heltec T114 into UF2 mode now:"
echo "    1. Plug in board."
echo "    2. Double-tap RST."
echo "    3. Wait for HT-n5262 drive."
read -r -p "Press ENTER after the HT-n5262 UF2 drive appears..."

echo "[*] Waiting for HT-n5262 UF2 block device"
dev=""
for _ in $(seq 1 40); do
    dev="$(find_uf2_device || true)"
    [[ -n "$dev" ]] && break
    sleep 1
done

[[ -n "$dev" ]] || {
    echo "[x] Could not find HT-n5262 UF2 drive." >&2
    lsblk -o NAME,LABEL,SIZE,FSTYPE,MOUNTPOINT || true
    exit 1
}

echo "[+] Found UF2 device: $dev"

existing_mount="$(lsblk -no MOUNTPOINT "$dev" | head -1 || true)"
target_mount="$MOUNT_DIR"
mounted_by_script=0

if [[ -n "$existing_mount" ]]; then
    target_mount="$existing_mount"
else
    sudo mkdir -p "$target_mount"
    sudo mount -t vfat "$dev" "$target_mount"
    mounted_by_script=1
fi

echo "[*] Copying UF2"
sudo cp -v "$UF2" "$target_mount/"
sync

if [[ "$mounted_by_script" -eq 1 ]]; then
    sudo umount "$target_mount" 2>/dev/null || true
else
    udisksctl unmount -b "$dev" 2>/dev/null || true
fi

echo "[+] Flash copy complete."
echo "Unplug the Heltec, wait 5 seconds, then plug it back in normally."
echo "Expected lab firmware runtime: /dev/ttyACM0 or /dev/ttyACM1"
