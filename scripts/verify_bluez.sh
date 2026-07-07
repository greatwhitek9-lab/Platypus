#!/usr/bin/env bash
set -euo pipefail

echo "[*] Platypus BlueZ verification"

sudo modprobe btusb || true
sudo systemctl restart bluetooth || true
sudo rfkill unblock bluetooth || true

echo
echo "=== bluetoothctl list ==="
bluetoothctl list || true

echo
echo "=== /sys/class/bluetooth ==="
ls /sys/class/bluetooth/ || true

echo
echo "=== lsusb ==="
lsusb

echo
echo "=== recent Bluetooth/USB dmesg ==="
dmesg | grep -iE 'usb|bluetooth|hci|btusb|zephyr|nrf|heltec|adafruit|ht-n5262|2fe3' | tail -120 || true
