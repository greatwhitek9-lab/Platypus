# Naughty Platypus One-Shot Installation

This document covers the one-shot installer for the Naughty Platypus branch.

The installer prepares the Linux host, builds the firmware, flashes the Heltec T114 / HT-n5262 board, verifies the runtime serial device, and can run a short host survey capture.

## Files

```text
install_naughty_platypus.sh
scripts/verify_naughty_platypus.sh
```

The installer expects the Naughty Platypus scaffold files to already exist:

```text
firmware/naughty-platypus/
tools/naughty_platypus_host.py
tools/inspect_uf2.py
tools/patch_uf2_family.py
```

## Board stitching

The installer preserves the Heltec T114 / HT-n5262 fixes discovered during flashing:

```text
CONFIG_USE_DT_CODE_PARTITION=n
CONFIG_FLASH_LOAD_OFFSET=0x1000
CONFIG_FLASH_LOAD_SIZE=0xdf000
UF2 family patched to 0x239a0071
```

Expected UF2:

```text
releases/naughty-platypus-HT-n5262-offset1000.uf2
```

Expected UF2 metadata:

```text
Address min: 0x00001000
Families: ['0x239a0071']
```

## Full one-shot

From the repo root:

```bash
chmod +x install_naughty_platypus.sh scripts/*.sh tools/*.py
./install_naughty_platypus.sh --install-deps
```

This will:

1. Install common Kali/Debian host dependencies.
2. Check Python serial support.
3. Build the Naughty Platypus UF2.
4. Patch the UF2 family to `0x239a0071`.
5. Prompt you to double-tap `RST` on the Heltec.
6. Copy the UF2 to the `HT-n5262` drive.
7. Prompt you to unplug/replug normally.
8. Verify `/dev/ttyACM*`.
9. Offer to run a short BLE survey capture.

## With Zephyr setup

If the normal Zephyr workspace is missing, run:

```bash
./install_naughty_platypus.sh --install-deps --setup-zephyr
```

Default expected Zephyr paths:

```text
WEST=$HOME/ble-dongle-build/.venv/bin/west
ZEPHYR_BASE=$HOME/ble-dongle-build/zephyrproject/zephyr
```

## Build only

```bash
./install_naughty_platypus.sh --build-only
```

## Flash existing UF2 only

```bash
./install_naughty_platypus.sh --skip-build
```

or:

```bash
./install_naughty_platypus.sh --flash-only
```

## Host setup only

```bash
./install_naughty_platypus.sh --install-deps --host-only
```

## Run host collector after install

```bash
./install_naughty_platypus.sh --run-host
```

Use a specific serial port:

```bash
./install_naughty_platypus.sh --run-host --port /dev/ttyACM0
```

## Verify after flashing

```bash
./scripts/verify_naughty_platypus.sh
```

or:

```bash
./scripts/verify_naughty_platypus.sh /dev/ttyACM0
```

## Runtime difference from standard Platypus

Standard Platypus HCI firmware:

```text
Zephyr USBD BT HCI
hci0 / hci1
BlueZ tools: bluetoothctl, btmgmt, btmon
```

Naughty Platypus lab firmware:

```text
/dev/ttyACM0 or /dev/ttyACM1
Host tool: tools/naughty_platypus_host.py
Serial shell: screen /dev/ttyACM0 115200
```

## Safety defaults

The one-shot installer builds the passive lab/survey scaffold. Restricted tools remain:

```text
default n
enabled=false
stub_only
not_implemented
```

The installer does not activate restricted placeholders.
