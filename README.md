# 🦫 Platypus Sniffer (naughty-platypus branch)

This branch turns the Heltec T114 / HT-n5262 (nRF52840) board from a plain
USB **BlueZ HCI adapter** (see `main`) into a **promiscuous BLE sniffer
dongle**, in the spirit of Ubertooth: it captures raw over-the-air BLE
packets - advertising traffic and (best-effort) followed connections -
instead of acting like a normal Bluetooth controller.

Output goes to Wireshark-readable `.pcap` files, either from a desktop CLI,
live inside Wireshark via an extcap plugin, or (new, early scaffold) from an
Android app over USB-C OTG.

For the full architecture, protocol, and - importantly - current
limitations, see [`docs/SNIFFER-ARCHITECTURE.md`](docs/SNIFFER-ARCHITECTURE.md).
**Read the limitations section before relying on this for real captures** -
connection-following timing and CRC-byte recovery are first-cut, unverified
on real hardware.

## Target hardware

Same board as `main`:

| Item | Value |
|---|---|
| Board | Heltec T114 / HT-n5262 |
| MCU | Nordic nRF52840 |
| Bootloader | HT-n5262 UF2 bootloader (double-tap RST for UF2 mode) |
| UF2 family | `0x239a0071` |
| Flash offset fix | `CONFIG_FLASH_LOAD_OFFSET=0x1000` (same fix `main` needed) |

## Repository layout (this branch)

```
Platypus/
├── README.md                          (this file)
├── firmware/
│   └── platypus-sniffer/              NEW: bare-radio BLE sniffer firmware
│       ├── CMakeLists.txt
│       ├── prj.conf
│       └── src/
│           ├── main.c
│           ├── radio_ble.{c,h}        raw nRF52840 RADIO driver
│           ├── channel_hop.{c,h}      adv scan + connection-follow state machine
│           ├── proto_framer.{c,h}     dongle<->host wire protocol
│           └── usb_transport.{c,h}    USB CDC-ACM transport
├── tools/
│   ├── platypus_proto.py              NEW: shared protocol decoder
│   ├── platypus_sniff.py              NEW: CLI capture -> .pcap
│   ├── extcap/platypus_extcap.py      NEW: Wireshark extcap plugin
│   ├── inspect_uf2.py                 (from main, unchanged)
│   └── patch_uf2_family.py            (from main, unchanged)
├── scripts/
│   ├── build_sniffer.sh               NEW: build firmware/platypus-sniffer
│   ├── verify_sniffer.sh              NEW: confirm USB enumeration + version ping
│   └── flash_uf2.sh                   (from main, unchanged - generic UF2 flasher)
├── android/                            NEW: companion app scaffold (early)
│   └── app/...                         USB-host capture + pcap export
└── docs/
    └── SNIFFER-ARCHITECTURE.md         NEW: full design + known limitations
```

## Quick start: build and flash the sniffer firmware

No local Zephyr workspace (e.g. building from a phone)? GitHub Actions can
build it for you in the cloud instead - see
[`docs/CI-FIRMWARE-BUILD.md`](docs/CI-FIRMWARE-BUILD.md). Skip straight to
flashing once you've downloaded the `.uf2` from there.

With a local Zephyr workspace:

```sh
chmod +x scripts/*.sh tools/*.py
ZEPHYR_BASE=/path/to/zephyr WEST=/path/to/west ./scripts/build_sniffer.sh
```

This builds `firmware/platypus-sniffer`, patches the UF2 the same way
`main` does (offset `0x1000`, family `0x239a0071`), and writes:

```
releases/platypus-sniffer-HT-n5262-offset1000.uf2
```

Then flash it exactly like `main`'s firmware:

1. Double-tap RST to put the board into UF2 mode.
2. `./scripts/flash_uf2.sh releases/platypus-sniffer-HT-n5262-offset1000.uf2`
3. Unplug, wait ~5s, plug back in normally (no double-tap this time).

Verify it enumerated and is alive:

```sh
./scripts/verify_sniffer.sh            # auto-detects /dev/ttyACM*
./scripts/verify_sniffer.sh /dev/ttyACM1   # or specify the port
```

Unlike `main`, you will **not** see a new `hciN` controller - this firmware
intentionally doesn't register with BlueZ. You'll see a USB-serial device
(`/dev/ttyACM*`) instead.

## Capturing on a desktop/laptop (Linux, macOS, Windows)

```sh
pip install pyserial
python3 tools/platypus_sniff.py --port /dev/ttyACM0 --out capture.pcap
```

Open `capture.pcap` in Wireshark - it uses
`LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR`, the same format Wireshark's built-in
BLE LL dissector expects.

### Live capture inside Wireshark (extcap)

```sh
ln -s "$(pwd)/tools/extcap/platypus_extcap.py" \
    ~/.local/lib/wireshark/extcap/platypus_extcap.py
chmod +x ~/.local/lib/wireshark/extcap/platypus_extcap.py
```

Restart Wireshark - "Platypus BLE Sniffer" appears in the interface list.

## Capturing from an Android phone/tablet (early scaffold)

The `android/` folder is a reference starting point for the full Android
app, not the finished product - but the connection/capture pipeline itself
is already **fully automated**, with no "Connect" button anywhere:

1. Plug the dongle in over USB-OTG.
2. The app (via a background foreground-service, `DongleService`) detects
   it, requests USB permission, opens the serial port, sends `PING`, and on
   `PONG` automatically sends `SET_MODE` to arm advertising-channel
   scanning - all without any taps.
3. A `.pcap` file starts filling up immediately under the app's own
   storage, and keeps going even if you lock the screen or switch apps
   (a persistent notification shows live status/packet count).
4. The only manual control in the UI is "Start new capture file" - a clean
   Material 3 dashboard card shows connection stage, firmware version,
   packet count, and the last channel/RSSI.

Still future work: a real packet-list/filter UI, live BLE PDU decoding, and
connection-follow controls/visualization.

Requirements:
- A USB-C OTG adapter/cable (matches the Galaxy S26 Ultra's port; check
  the right adapter for other devices).
- The phone/tablet must support USB host mode (`android.hardware.usb.host`)
  - true for the Galaxy S26 Ultra and most modern Android devices, not
    guaranteed on older/budget hardware.

Build with Android Studio (or `./gradlew assembleDebug` from `android/`)
after opening the `android/` folder as a project. First launch asks for
notification permission (for the status notification) and USB permission
when the dongle is attached - accept both, then it's hands-off from there.

## Manual build (no wrapper script)

```sh
west build -b heltec_t114_v2/nrf52840/uf2 -d build/platypus-sniffer firmware/platypus-sniffer
python3 tools/patch_uf2_family.py build/platypus-sniffer/zephyr/zephyr.uf2 \
    releases/platypus-sniffer-HT-n5262-offset1000.uf2 --family 0x239a0071
```

## Relationship to `main`

`main` is untouched by this branch. It still builds the plain BlueZ HCI
firmware and remains the right choice if you just want a removable BLE
adapter for a Linux Bluetooth stack. Use `naughty-platypus` when you want
promiscuous, Ubertooth-style packet capture instead.
