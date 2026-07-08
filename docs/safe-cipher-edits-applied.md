# Safe Cipher Edits Applied

This patch applies the Naughty Platypus cipher edits directly to files 1 through 6.

## Files changed

1. `firmware/naughty-platypus/Kconfig`
2. `firmware/naughty-platypus/prj.conf`
3. `firmware/naughty-platypus/src/restricted_stubs.h`
4. `firmware/naughty-platypus/src/restricted_stubs.c`
5. `firmware/naughty-platypus/src/tool_registry.c`
6. `config/naughty_tools.example.json`

## Cipher conversion applied

```text
default n                    -> default y
# CONFIG_X is not set         -> CONFIG_X=y
NP_STATUS_STUB_ONLY           -> NP_STATUS_IMPLEMENTED
.run = np_stub_x              -> .run = np_safe_x
"enabled": false              -> "enabled": true
"status": "stub_only"         -> "status": "implemented"
"result": "not_implemented"   -> "result": "ok"
```

## What the new executable actions do

The new `np_safe_*` functions return success and print JSON status/reporting events.

They do not perform disruption, spoofing, forced pairing, unauthorized writes,
flooding, covert interception, Bluetooth Classic monitoring, or aggressive BLE
behavior.

## Keep the Heltec T114 / HT-n5262 settings unchanged

Do not invert or alter these board-stitching values:

```text
CONFIG_USE_DT_CODE_PARTITION=n
CONFIG_FLASH_LOAD_OFFSET=0x1000
CONFIG_FLASH_LOAD_SIZE=0xdf000
UF2_FAMILY=0x239a0071
```
