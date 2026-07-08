# Naughty Platypus Patch

This patch adds an Ubertooth-style BLE lab suite scaffold for the Heltec T114 / HT-n5262 / nRF52840.

## Important

The build scripts include the tested T114/HT-n5262 fixes:

```text
CONFIG_USE_DT_CODE_PARTITION=n
CONFIG_FLASH_LOAD_OFFSET=0x1000
CONFIG_FLASH_LOAD_SIZE=0xdf000
UF2 family patched to 0x239a0071
```

## Files added

```text
firmware/naughty-platypus/
scripts/build_naughty_platypus.sh
scripts/flash_naughty_platypus.sh
tools/naughty_platypus_host.py
config/naughty_tools.example.json
docs/naughty-platypus-ubertooth-style.md
docs/manual-activation-map.html
```

## Local push

```bash
cd ~/Desktop/Platypus
git fetch origin
git checkout naughty-platypus

unzip ~/Downloads/naughty_platypus_ubertooth_style_patch.zip -d /tmp/naughty_patch
cp -a /tmp/naughty_patch/. .

chmod +x scripts/build_naughty_platypus.sh scripts/flash_naughty_platypus.sh tools/naughty_platypus_host.py

git add firmware/naughty-platypus \
        scripts/build_naughty_platypus.sh \
        scripts/flash_naughty_platypus.sh \
        tools/naughty_platypus_host.py \
        config/naughty_tools.example.json \
        docs/naughty-platypus-ubertooth-style.md \
        docs/manual-activation-map.html \
        README-naughty-platypus-patch.md

git commit -m "Add Naughty Platypus Ubertooth-style BLE lab scaffold"
git push origin naughty-platypus
```
