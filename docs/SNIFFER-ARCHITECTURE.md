# Platypus Sniffer architecture (naughty-platypus branch)

This branch changes what the Heltec T114 / HT-n5262 *is*: on `main` it's a
conformant BlueZ USB HCI controller; here it's a promiscuous BLE sniffer in
the spirit of Ubertooth - it captures raw over-the-air BLE packets instead
of only packets addressed to a paired host.

## Firmware (`firmware/platypus-sniffer/`)

- `CONFIG_BT=n` - Zephyr's Bluetooth host/controller stack is **not** used.
- `src/radio_ble.c` drives the nRF52840 `RADIO` peripheral directly in its
  native `BLE_1MBIT` hardware mode (preamble detection, de-whitening, CRC
  check all done in silicon - same as what a real BLE controller uses).
- `src/channel_hop.c` is the capture state machine:
  - **Advertising scan**: round-robins RF channels 37/38/39 using the fixed
    advertising access address (`0x8E89BED6`) and CRC init (`0x555555`).
  - **Connection follow**: when a `CONNECT_IND` PDU is seen, its `AA`,
    `CRCInit`, channel map and hop increment are parsed out and used to
    compute the BLE data-channel-selection-algorithm #1 hop sequence
    (Core spec Vol 6, Part B, 4.5.8.2), so the dongle can keep listening to
    one specific connection's data channel traffic.
- `src/proto_framer.c` + `src/usb_transport.c` stream every captured packet
  to the host over USB CDC-ACM using a small framed protocol (see below) -
  intentionally not HCI, since HCI assumes a conformant controller.

### Known limitations (read before relying on this for real work)

- **Connection-follow timing is a first cut.** It retunes the radio
  reactively (on each received packet) rather than from a `TIMER1`-scheduled
  connection-event anchor. It will drift/lose lock during silent intervals
  on a connection (keep-alives, idle periods). Tightening this needs a real
  board: measure anchor-point drift and add a scheduled retune via PPI/TIMER
  compare events instead of "retune after RX".
- **CRC bytes aren't recovered.** The nRF52840 radio strips the 3-byte CRC
  from the RX buffer and only exposes a pass/fail (`CRCSTATUS`); this
  firmware reports `crc_ok` but pads the pcap's CRC field with zeroes rather
  than reconstructing the real CRC. Wireshark's BLE LL dissector still
  decodes the packet; a CRC-accuracy purist would want to compute/replay the
  real CRC bytes here.
- **No RF testing has been done on real hardware as part of this change.**
  The radio register configuration, channel math and whitening IV follow the
  public Bluetooth Core Specification directly, but only a board in hand can
  confirm timing margins, RSSI calibration, and actual capture reliability.
  Validate with a known BLE peripheral (a fitness band, a BLE dev kit
  advertising on a timer, etc.) and a reference capture (nRF Sniffer, or a
  second Ubertooth) before trusting results.

## Wire protocol (dongle <-> host)

```
0xAA 0x55 | len:u16 LE | type:u8 | payload[len-1] | crc16:u16 LE
```

Message types: `CAPTURE(0x01)`, `PING(0x02)`, `PONG(0x03)`,
`SET_MODE(0x04)`, `VERSION(0x05)`. See `firmware/platypus-sniffer/src/proto_framer.h`
for the authoritative list and `tools/platypus_proto.py` /
`android/app/.../PlatypusProto.kt` for host-side decoders - **keep all
three in sync** if you change the framing.

## Host tooling (`tools/`)

- `platypus_sniff.py` - CLI: dongle -> `.pcap` file
  (`LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR`, opens directly in Wireshark).
- `extcap/platypus_extcap.py` - Wireshark extcap plugin so the dongle shows
  up as a live capture interface inside Wireshark itself.

## Android app (`android/`)

A phone/tablet is just another USB host, so the same dongle/firmware works
there with no firmware changes - the Android side uses
[usb-serial-for-android](https://github.com/mik3y/usb-serial-for-android) to
talk CDC-ACM purely through Android's USB host API, which avoids depending
on any OEM's kernel CDC driver being enabled. That's what makes this
portable across devices (Samsung Galaxy S26 Ultra and other Android
phones/tablets with USB-host/OTG support) rather than tied to one specific
model.

What's in this branch so far is a **reference scaffold**, not the finished
app, but the control flow is already fully automated end to end:

- `DongleController` runs the whole lifecycle with no manual trigger:
  find device -> request permission -> open port -> send `PING` -> on
  `PONG` send `SET_MODE` to arm advertising-channel scanning -> start
  writing a pcap file. Any failure (unplug, read error, handshake timeout)
  drops it back to "waiting" and the same chain retries automatically next
  time a device is present.
- `DongleService` is a foreground service hosting that controller, so
  capture keeps running screen-off/backgrounded, with a persistent status
  notification.
- `MainActivity` is a thin Compose UI that just binds to the service and
  renders its `StateFlow<DongleUiState>` - a status card (stage, firmware
  version) and a stats card (packet count, last channel/RSSI). The only
  user-facing control is "Start new capture file"; connecting and arming
  the dongle are never user actions.
- Still missing: a real packet-list/filter UI, live BLE PDU decoding, a
  share/export flow, and connection-follow visualization. Those are the
  "later, full app" work.

Device-side requirements for any Android device:
- Must support USB host mode / OTG (`android.hardware.usb.host`). Most
  modern phones including the Galaxy S26 Ultra do; some budget/older models
  don't, or need a brand-specific OTG adapter.
- A USB-C OTG adapter/cable matching the phone's port (USB-C on the S26
  Ultra) and the dongle's connector.
- The firmware sets a conservative bus-powered current request (96 mA, see
  `firmware/platypus-sniffer/prj.conf`) so phone-side OTG power budgeting
  doesn't reject or throttle it.

## Why not just extend `main`'s HCI firmware?

A conformant BLE controller (what `main` builds) only hands a host packets
addressed to it, post-processed by the controller's own link layer rules.
Promiscuous capture needs the opposite: see every valid PDU on a channel
regardless of address, and be able to retune/follow connections that aren't
"ours." That requires bypassing the host/controller boundary entirely,
which is why this lives as its own firmware app and branch rather than a
flag on the HCI build.
