#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UF2="${UF2:-$REPO_ROOT/build/platypus-heltec-t114-hci-usb/zephyr/zephyr.uf2}"
MOUNTPOINT="${MOUNTPOINT:-/mnt/t114}"

echo "[*] Platypus UF2 flasher"
echo "[*] UF2: $UF2"

if [[ ! -f "$UF2" ]]; then
  echo "[!] UF2 file not found: $UF2"
  echo "[!] Run: bash scripts/build.sh"
  exit 1
fi

DEVICE="${DEVICE:-$(lsblk -nrpo NAME,LABEL,FSTYPE | awk 'tolower($2) == "ht-n5262" && $3 == "vfat" {print $1; exit}')}"

if [[ -z "$DEVICE" ]]; then
  echo "[!] Could not find HT-n5262 UF2 drive."
  echo "[!] Double-tap RST on the Heltec T114, then run:"
  echo "    lsblk -o NAME,LABEL,SIZE,FSTYPE,MOUNTPOINT"
  exit 1
fi

echo "[+] Found UF2 device: $DEVICE"

sudo mkdir -p "$MOUNTPOINT"

if ! mountpoint -q "$MOUNTPOINT"; then
  sudo mount -t vfat "$DEVICE" "$MOUNTPOINT"
fi

echo "[*] UF2 drive contents:"
ls -lah "$MOUNTPOINT"

echo "[*] Copying Platypus firmware..."
sudo cp -v "$UF2" "$MOUNTPOINT/"
sync

sudo umount "$MOUNTPOINT" 2>/dev/null || true

echo "[+] Flash copy complete."
echo "[*] The board should reboot/disconnect."
echo "[*] Unplug/replug the T114 normally, then verify BlueZ."
