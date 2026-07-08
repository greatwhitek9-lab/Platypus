# Naughty Platypus: Ubertooth-Style BLE Lab Suite

This branch adds an Ubertooth-inspired BLE lab firmware scaffold for the Heltec T114 / HT-n5262 / nRF52840.

It is not a direct Ubertooth replacement. The nRF52840 with Zephyr is useful for BLE advertisement observation, RSSI survey, host-side logging, and metadata analysis. It does not provide Bluetooth Classic BR/EDR monitoring, and this public scaffold does not include connection-following, disruption, forced pairing, spoofing, or unauthorized GATT mutation.

## T114 / HT-n5262 build stitching

The build script implements the same fixes that worked on your Heltec T114:

```text
CONFIG_USE_DT_CODE_PARTITION=n
CONFIG_FLASH_LOAD_OFFSET=0x1000
CONFIG_FLASH_LOAD_SIZE=0xdf000
UF2 family patched to 0x239a0071
```

Expected UF2 output:

```text
releases/naughty-platypus-HT-n5262-offset1000.uf2
```

Expected patched metadata:

```text
Address min: 0x00001000
Families: ['0x239a0071']
```

## Implemented

- Passive BLE advertisement survey.
- RSSI counters.
- Advertisement name parsing.
- Flags parsing.
- TX power parsing.
- Manufacturer-data preview.
- 16-bit service-data preview.
- JSON event stream over USB CDC ACM.
- Host CSV / JSONL collector.

## Stub-only restricted placeholders

| Tool | Enabled | Status |
|---|---:|---|
| `ble_channel_survey` | false | stub only |
| `ble_connection_follow` | false | stub only |
| `ble_gatt_mutation_lab` | false | stub only |
| `ble_pairing_security_lab` | false | stub only |
| `ble_advertising_tx_lab` | false | stub only |
| `ble_stability_stress_lab` | false | stub only |
| `classic_bt_monitor` | false | unsupported/stub only |

## Build

```bash
chmod +x scripts/build_naughty_platypus.sh scripts/flash_naughty_platypus.sh tools/naughty_platypus_host.py
./scripts/build_naughty_platypus.sh
```

## Flash

```bash
./scripts/flash_naughty_platypus.sh
```

Or manually copy:

```text
releases/naughty-platypus-HT-n5262-offset1000.uf2
```

to the `HT-n5262` UF2 drive.

## Use

```bash
python3 -m pip install pyserial

mkdir -p captures
python3 tools/naughty_platypus_host.py \
  --port /dev/ttyACM0 \
  --duration 120 \
  --csv captures/naughty-survey.csv \
  --jsonl captures/naughty-survey.jsonl
```

Manual shell:

```bash
screen /dev/ttyACM0 115200
```

Commands:

```text
np tools_list
np scan_on
np status
np reset_stats
np scan_off
np tools_run ble_passive_survey
np tools_run ble_connection_follow
```

Running a restricted placeholder returns `enabled=false` / `stub_only`.
