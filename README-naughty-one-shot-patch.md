# Naughty Platypus One-Shot Installer Patch

This patch adds a root-level one-shot installer for the Naughty Platypus branch.

## Adds

```text
install_naughty_platypus.sh
scripts/verify_naughty_platypus.sh
docs/naughty-one-shot-install.md
README-naughty-one-shot-patch.md
```

## Full install

```bash
chmod +x install_naughty_platypus.sh scripts/*.sh tools/*.py
./install_naughty_platypus.sh --install-deps
```

## Full install and Zephyr setup

```bash
./install_naughty_platypus.sh --install-deps --setup-zephyr
```

## Push to branch

```bash
cd ~/Desktop/Platypus
git fetch origin
git checkout naughty-platypus

unzip ~/Downloads/naughty_platypus_one_shot_patch.zip -d /tmp/naughty_one_shot
cp -a /tmp/naughty_one_shot/. .

chmod +x install_naughty_platypus.sh scripts/verify_naughty_platypus.sh

git add install_naughty_platypus.sh \
        scripts/verify_naughty_platypus.sh \
        docs/naughty-one-shot-install.md \
        README-naughty-one-shot-patch.md

git commit -m "Add Naughty Platypus one-shot installer"
git push origin naughty-platypus
```

## T114 / HT-n5262 fixes

The installer preserves:

```text
CONFIG_USE_DT_CODE_PARTITION=n
CONFIG_FLASH_LOAD_OFFSET=0x1000
CONFIG_FLASH_LOAD_SIZE=0xdf000
UF2 family patched to 0x239a0071
```
