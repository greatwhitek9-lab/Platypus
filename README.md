<p align="center">
  <img src="docs/images/naughty-platypus-banner.jpg" alt="Naughty Platypus" width="100%">
</p>

<h1 align="center">Naughty Platypus</h1>

<p align="center">
  <strong>An Ubertooth-inspired Bluetooth Low Energy survey firmware for the Heltec T114 / HT-n5262.</strong>
</p>

<p align="center">
  Active scanning by default · Passive mode available · Queue-based processing · Structured JSON output · One-word commands
</p>

> [!IMPORTANT]
> Naughty Platypus is intended for authorized Bluetooth Low Energy discovery, asset inventory, troubleshooting, education, and defensive laboratory research. It is not a jammer, denial-of-service tool, packet injector, key-recovery platform, or full Ubertooth One replacement.

---

## What is Naughty Platypus?

Naughty Platypus is the specialized BLE survey branch of the Platypus project. It turns the **Heltec T114 / HT-n5262 nRF52840** into a USB-connected BLE discovery and telemetry device running Zephyr RTOS.

Instead of appearing to Linux as a normal BlueZ HCI controller, Naughty Platypus runs its own firmware-side scanning engine and streams structured survey records over USB CDC serial.

Current capabilities include:

- Active BLE scanning by default, including scan-response discovery when supported by nearby devices.
- Optional passive scanning.
- MAC/address, address type, RSSI, advertisement type, payload length, local-name, manufacturer-data, and 16-bit service-data summaries.
- Queue-based event processing that keeps the Bluetooth receive callback small and stable.
- Continuous JSON-line output suitable for terminal viewing, log capture, scripts, and future dashboards.
- One-word firmware commands such as `scan`, `stop`, `active`, `passive`, `mode`, and `status`.
- HT-n5262 UF2 offset and family-ID corrections required by the tested board bootloader.

---

## Platypus vs. Naughty Platypus

The regular **Platypus** firmware and **Naughty Platypus** target different workflows.

| Capability | Platypus | Naughty Platypus |
|---|---|---|
| Primary role | USB BLE HCI adapter | Standalone BLE survey firmware |
| Linux interface | BlueZ HCI controller such as `hci1` | USB CDC serial such as `/dev/ttyACM0` |
| Main processing location | Linux/BlueZ host | Heltec firmware plus optional host scripts |
| Typical tools | `bluetoothctl`, `btmgmt`, `btmon` | Serial terminal, JSONL logging, host parser |
| Active scan default | Controlled by host application | Yes |
| Passive scan option | Controlled by host application | Yes, one-word command |
| Firmware-side advertisement parser | No | Yes |
| Queue-based survey engine | No | Yes |
| Device name and address summaries | Through BlueZ tools | Direct JSON output |
| Manufacturer/service summaries | Through host tools | Direct JSON fields |
| One-word firmware commands | No | Yes |
| Best use | General Linux BLE adapter | Dedicated BLE observation and inventory |
| Full Ubertooth raw-radio replacement | No | No |

### Choose regular Platypus when

- You want the Heltec board to appear as a normal Linux Bluetooth controller.
- You want BlueZ applications to directly control scanning and connections.
- You primarily use `bluetoothctl`, `btmgmt`, `btmon`, or another HCI-based application.

### Choose Naughty Platypus when

- You want the board to continuously survey nearby BLE advertisements itself.
- You want structured JSON output over serial.
- You want a lightweight dedicated BLE inventory sensor.
- You want active and passive scan modes controlled by simple firmware commands.
- You want to build host-side logging, dashboards, device caches, or GPS-tagged survey workflows.

---

## Supported hardware

| Item | Value |
|---|---|
| Board | Heltec T114 / HT-n5262 |
| MCU | Nordic nRF52840 |
| RTOS | Zephyr |
| Bootloader | HT-n5262 UF2 bootloader |
| UF2 boot mode | Double-tap `RST` |
| Runtime USB interface | CDC ACM serial |
| Typical runtime device | `/dev/ttyACM0` |
| Build target | `heltec_t114_v2/nrf52840/uf2` |
| Application offset | `0x1000` |
| Application size | `0xdf000` |
| UF2 family | `0x239a0071` |
| Tested host families | Kali Linux, Parrot OS, Debian-derived Linux |

Expected release file:

```text
releases/naughty-platypus-HT-n5262-offset1000.uf2
```

---

## Firmware architecture

```text
Nearby BLE advertisements and scan responses
                    │
                    ▼
             nRF52840 radio
                    │
                    ▼
          Zephyr BLE scan callback
          address + RSSI + small
          bounded payload copy only
                    │
                    ▼
             Message queue
                    │
                    ▼
        Main-loop advertisement parser
          name / manufacturer / service
                    │
                    ▼
         Structured JSON-line output
                    │
                    ▼
             USB CDC serial
                    │
                    ▼
         Kali / Parrot host terminal
```

The Bluetooth callback deliberately avoids expensive parsing and printing. It copies a bounded record into a message queue and returns quickly. Parsing and serial output happen later in the main loop, which prevents the scanner from freezing under moderate advertisement traffic.

---

# Kali Linux and Parrot OS quick start

The same commands work on both Kali Linux and Parrot OS unless otherwise noted.

## 1. Install host dependencies

```bash
sudo apt update
sudo apt install -y \
  git python3 python3-venv python3-pip \
  cmake ninja-build gperf ccache \
  device-tree-compiler wget curl xz-utils file \
  make gcc g++ usbutils util-linux screen minicom
```

For a full local Zephyr build, you also need a working Zephyr workspace and `west`. The existing project defaults are:

```text
ZEPHYR_BASE=$HOME/ble-dongle-build/zephyrproject/zephyr
WEST=$HOME/ble-dongle-build/.venv/bin/west
```

## 2. Clone the Naughty Platypus branch

```bash
git clone --branch naughty-platypus --single-branch \
  https://github.com/greatwhitek9-lab/Platypus.git
cd Platypus
```

If the repository already exists locally:

```bash
cd ~/Desktop/Platypus
git fetch origin
git switch naughty-platypus
git pull --rebase origin naughty-platypus
```

## 3. Make project scripts executable

```bash
chmod +x scripts/*.sh tools/*.py install_naughty_platypus.sh
```

## 4. Build the firmware

```bash
rm -rf build/naughty-platypus-offset1000
./scripts/build_naughty_platypus.sh 2>&1 | tee /tmp/naughty_build.log
```

The build script should produce:

```text
releases/naughty-platypus-HT-n5262-offset1000.uf2
```

Expected UF2 inspection values:

```text
Address min: 0x00001000
Families: ['0x239a0071']
```

### One-line build command

```bash
cd ~/Desktop/Platypus && rm -rf build/naughty-platypus-offset1000 && ./scripts/build_naughty_platypus.sh 2>&1 | tee /tmp/naughty_build.log
```

### Show useful build errors

```bash
grep -n -i "error:\|warning:\|undefined symbol\|failed" /tmp/naughty_build.log | tail -120
```

## 5. Flash the Heltec T114

Run:

```bash
./scripts/flash_naughty_platypus.sh
```

When prompted:

1. Double-tap the Heltec `RST` button.
2. Wait for the removable drive labeled `HT-n5262`.
3. Allow the script to copy and sync the UF2.
4. Unplug the board.
5. Wait about five seconds.
6. Plug it back in normally without double-tapping reset.

### One-line build and flash

```bash
cd ~/Desktop/Platypus && rm -rf build/naughty-platypus-offset1000 && ./scripts/build_naughty_platypus.sh 2>&1 | tee /tmp/naughty_build.log && ./scripts/flash_naughty_platypus.sh
```

## 6. Confirm the runtime serial device

```bash
lsusb
ls -l /dev/ttyACM*
```

The board normally appears as:

```text
/dev/ttyACM0
```

If your user cannot access the serial device without `sudo`, add the user to the serial-access group:

```bash
sudo usermod -aG dialout "$USER"
```

Log out and back in before testing the new group membership.

---

# Viewing live survey output

## Direct terminal command

```bash
cd ~/Desktop/Platypus
PORT="$(ls /dev/ttyACM* 2>/dev/null | head -n1)"
echo "Using $PORT"
sudo timeout 120 bash -c "
  stty -F $PORT 115200 raw -echo -crtscts 2>/dev/null || true
  cat $PORT
"
```

## One-line terminal command

```bash
PORT="$(ls /dev/ttyACM* 2>/dev/null | head -n1)" && echo "Using $PORT" && sudo timeout 120 bash -c "stty -F $PORT 115200 raw -echo -crtscts 2>/dev/null || true; cat $PORT"
```

## Using `screen`

```bash
sudo screen /dev/ttyACM0 115200
```

Exit `screen` with:

```text
Ctrl-A, then K, then Y
```

## Using `minicom`

```bash
sudo minicom -D /dev/ttyACM0 -b 115200
```

---

# Saving a BLE survey log

The firmware emits one JSON object per line, which can be captured as JSONL.

```bash
cd ~/Desktop/Platypus
mkdir -p captures
PORT="$(ls /dev/ttyACM* 2>/dev/null | head -n1)"
OUT="captures/ble_survey_$(date +%Y%m%d_%H%M%S).jsonl"
echo "Saving $PORT to $OUT"
sudo bash -c "
  stty -F $PORT 115200 raw -echo -crtscts 2>/dev/null || true
  timeout 300 cat $PORT
" | tee "$OUT"
```

One-line five-minute capture:

```bash
cd ~/Desktop/Platypus && mkdir -p captures && PORT="$(ls /dev/ttyACM* 2>/dev/null | head -n1)" && OUT="captures/ble_survey_$(date +%Y%m%d_%H%M%S).jsonl" && echo "Saving $PORT to $OUT" && sudo bash -c "stty -F $PORT 115200 raw -echo -crtscts 2>/dev/null || true; timeout 300 cat $PORT" | tee "$OUT"
```

View the latest useful records:

```bash
LATEST="$(ls -t captures/ble_survey_*.jsonl | head -n1)"
grep '"adv_summary"\|"survey_status"\|"queue_status"\|"scan_mode"' "$LATEST" | tail -50
```

---

# One-word firmware commands

Type these directly into an interactive serial terminal, one command per line.

| Command | Action |
|---|---|
| `version` | Print firmware application and build version |
| `status` | Print scanner, event, RSSI, and queue statistics |
| `survey` | Drain queued advertisement summaries and print status |
| `scan` | Start scanning using the currently selected mode |
| `stop` | Stop scanning |
| `active` | Switch to active scanning and restart the scanner if necessary |
| `passive` | Switch to passive scanning and restart the scanner if necessary |
| `mode` | Show the selected active/passive mode |
| `reset` | Clear counters and purge queued records |
| `commands` | List the available one-word commands |

Active scan is the firmware default.

### Active vs. passive scanning

**Active scanning** sends standard BLE scan requests to scannable advertisers. This may produce scan-response records containing a local name or additional service/manufacturer data.

**Passive scanning** listens without sending scan requests. It is quieter but often discovers fewer device names.

Some devices will still show an empty name because they:

- Do not advertise a local name.
- Use manufacturer-specific payloads only.
- Use rotating random addresses.
- Advertise the name intermittently.
- Require a connection before exposing identifying information.

---

# Sending commands from another terminal

Keep one terminal open reading `/dev/ttyACM0`, then use a second terminal to send a command.

Show mode:

```bash
PORT="$(ls /dev/ttyACM* 2>/dev/null | head -n1)"
printf '\r\nmode\r\n' | sudo tee "$PORT" >/dev/null
```

Switch to active mode:

```bash
PORT="$(ls /dev/ttyACM* 2>/dev/null | head -n1)"
printf '\r\nactive\r\n' | sudo tee "$PORT" >/dev/null
```

Switch to passive mode:

```bash
PORT="$(ls /dev/ttyACM* 2>/dev/null | head -n1)"
printf '\r\npassive\r\n' | sudo tee "$PORT" >/dev/null
```

Show status:

```bash
PORT="$(ls /dev/ttyACM* 2>/dev/null | head -n1)"
printf '\r\nstatus\r\n' | sudo tee "$PORT" >/dev/null
```

List firmware commands:

```bash
PORT="$(ls /dev/ttyACM* 2>/dev/null | head -n1)"
printf '\r\ncommands\r\n' | sudo tee "$PORT" >/dev/null
```

---

# Example output

Boot and scan startup:

```json
{"type":"serial_boot","app":"naughty-platypus","path":"direct_cdc"}
{"type":"firmware_marker","build":"ble_active_default_v5"}
{"type":"scan_diag","step":"start","scan_type":1,"mode":"active","interval":160,"window":48}
{"path":"direct_cdc","type":"scan_start","err":0}
```

Advertisement summary:

```json
{"type":"adv_summary","addr":"B8:27:EB:E8:70:0A (public)","name":"KB-BLE-TEST","rssi":-67,"adv_type":0,"data_len":18,"mfg":"","svc16":"0f18","drained_events":42}
```

Status records:

```json
{"type":"survey_status","scanning":true,"scan_mode":"active","adv_events":828,"named_events":4,"mfg_events":30,"svc_events":12,"strongest_rssi":-61,"weakest_rssi":-95}
{"type":"queue_status","queued_events":828,"dropped_events":0,"drained_events":828,"pending":0}
```

Address notes:

```text
(public) = public or public-identity address reported by the Bluetooth stack
(random) = random static, resolvable private, or non-resolvable private address
```

Do not assume a randomized address permanently identifies a physical device.

---

# Test advertiser using a Raspberry Pi or Linux adapter

A second Linux machine can advertise a clearly named test payload with BlueZ `btmgmt`.

Install BlueZ if necessary:

```bash
sudo apt update
sudo apt install -y bluez
```

Configure the adapter:

```bash
sudo btmgmt power off
sudo btmgmt le on
sudo btmgmt bredr off
sudo btmgmt connectable on
sudo btmgmt power on
```

Advertise the test name `KB-BLE-TEST` in the primary advertising data:

```bash
sudo btmgmt advertising off 2>/dev/null || true
sudo btmgmt clr-adv 2>/dev/null || true
sudo btmgmt add-adv -d 02010603030f180c094b422d424c452d54455354 1
sudo btmgmt advertising on
```

Verify:

```bash
sudo btmgmt info
sudo btmgmt advinfo 2>/dev/null || true
```

Stop the test advertiser:

```bash
sudo btmgmt advertising off
```

---

# Troubleshooting on Kali and Parrot

## No `/dev/ttyACM0`

Check both normal and bootloader enumeration:

```bash
lsusb
lsblk -o NAME,LABEL,SIZE,FSTYPE,MOUNTPOINT
sudo dmesg --follow
```

If the drive label is `HT-n5262`, the board is still in UF2 bootloader mode. Unplug it and reconnect normally.

## Permission denied

Temporarily use `sudo`, or add your account to `dialout`:

```bash
sudo usermod -aG dialout "$USER"
```

## Serial device is busy

Find the process using it:

```bash
sudo lsof /dev/ttyACM0
sudo fuser -v /dev/ttyACM0
```

Close `screen`, `minicom`, another `cat`, or any ModemManager session that has opened the device.

## ModemManager interferes

Kali or Parrot may probe a new CDC serial device as a modem. Stop it temporarily:

```bash
sudo systemctl stop ModemManager 2>/dev/null || true
```

Disable it only when you do not need cellular modem management:

```bash
sudo systemctl disable --now ModemManager
```

## Names are blank

Blank names are normal. Confirm active mode:

```text
mode
active
```

Then test with the known `KB-BLE-TEST` Linux advertiser shown above.

## Queue drops rise quickly

Check:

```text
status
```

If `dropped_events` rises rapidly:

- Reduce serial output.
- Use passive mode in dense environments.
- Increase the drain rate carefully.
- Aggregate records by address on the host instead of printing every event indefinitely.

## Build failed

```bash
grep -n -i "error:\|warning:\|undefined symbol\|failed" /tmp/naughty_build.log | tail -120
```

---

# Repository layout

```text
Platypus/
├── README.md
├── install_naughty_platypus.sh
├── firmware/
│   └── naughty-platypus/
│       ├── CMakeLists.txt
│       ├── Kconfig
│       ├── app.overlay
│       ├── prj.conf
│       └── src/
│           ├── main.c
│           ├── passive_survey.c
│           └── passive_survey.h
├── scripts/
│   ├── build_naughty_platypus.sh
│   └── flash_naughty_platypus.sh
├── tools/
│   ├── inspect_uf2.py
│   ├── patch_uf2_family.py
│   └── naughty_platypus_host.py
├── docs/
│   └── images/
│       └── naughty-platypus-banner.jpg
└── releases/
    └── naughty-platypus-HT-n5262-offset1000.uf2
```

Generated captures, UF2 releases, local builds, and editor backups should remain untracked unless intentionally published as a release artifact.

---

# Current feature status

## Implemented

- Stable queue-based BLE advertisement collection.
- Active scan as the default mode.
- Optional passive mode.
- Runtime active/passive switching.
- MAC/address and address-type output.
- RSSI, advertisement type, and payload-length output.
- Local-name extraction when advertised.
- Manufacturer-data preview.
- 16-bit service-data preview.
- Queue depth and drop statistics.
- Strongest and weakest observed RSSI.
- One-word serial commands.
- HT-n5262 offset and UF2-family patching.

## Planned

- Per-device cache and duplicate suppression.
- Strongest-device and recently-seen summaries.
- Advertising interval estimation.
- BLE advertising-channel statistics where supported by the controller API.
- Manufacturer ID lookup on the host.
- iBeacon, Eddystone, AltBeacon, and Fast Pair parsing.
- CSV and analysis-friendly capture export.
- Interactive terminal UI.
- GPS tagging and database-backed survey sessions.

---

# Ubertooth comparison

Naughty Platypus borrows the workflow concept of a dedicated Bluetooth observation device, but its hardware and radio architecture differ from Ubertooth One.

Naughty Platypus currently focuses on BLE advertisement and scan-response discovery through the Nordic/Zephyr BLE stack. It does not provide arbitrary raw 2.4 GHz capture, Bluetooth Classic baseband monitoring, jamming, packet injection, or connection-key recovery.

The practical goal is:

```text
Ubertooth-inspired BLE survey workflow
not
Ubertooth One hardware emulation
```

---

# Responsible use

Use Naughty Platypus only:

- On equipment and networks you own.
- In environments where you have explicit authorization.
- For defensive discovery, inventory, troubleshooting, education, and research.
- In compliance with applicable privacy, radio, and computer-access laws.

The project intentionally excludes disruptive radio behavior, forced disconnections, unauthorized connection attempts, credential attacks, and denial-of-service functionality.

---

# Credits

- Zephyr Project
- Nordic Semiconductor
- Heltec Automation
- BlueZ
- GreatWhiteK9 Lab
- Urban Poacher

---

## Branch

This documentation describes:

```text
naughty-platypus
```

To return to the regular USB HCI version, switch to the main/default Platypus branch and follow that branch's README.
