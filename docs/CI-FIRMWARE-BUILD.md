# Building firmware without a local Zephyr workspace

`.github/workflows/build-sniffer-firmware.yml` builds
`firmware/platypus-sniffer` in GitHub's cloud runners (using the official
Zephyr project CI container, which already has the SDK/toolchain
installed), patches the UF2 the same way `scripts/build_sniffer.sh` does
locally, and uploads the finished file as a downloadable artifact. This is
the recommended path if you're working from a phone (Termux/Android) or any
machine where setting up a full Zephyr SDK toolchain isn't practical.

## Triggering a build

It runs automatically on every push to `naughty-platypus` that touches the
firmware. You can also trigger it manually:

1. On GitHub, go to the repo's **Actions** tab.
2. Select **Build Platypus Sniffer firmware** in the left sidebar.
3. Click **Run workflow** → **Run workflow** (branch: `naughty-platypus`).

## Downloading the result

1. Open the finished run (green checkmark) under the **Actions** tab.
2. Scroll to **Artifacts** at the bottom of the run summary page.
3. Tap **platypus-sniffer-uf2** to download it - on a phone this saves into
   your normal Downloads folder (browser-dependent; on GitHub's mobile web
   UI it's a plain file download).
4. Unzip it (artifacts are delivered as a `.zip` wrapping the one file) to
   get `platypus-sniffer-HT-n5262-offset1000.uf2`.
5. Flash it exactly like a locally-built UF2: double-tap RST on the board to
   enter UF2 mode, then copy the file onto the drive that appears (your
   phone's file manager can do this once the board is plugged in via OTG
   and mounted as a USB drive - Samsung's "My Files" app supports browsing
   USB-attached storage).

## If the build fails with "board not found"

The workflow starts from a clean, plain upstream Zephyr checkout each run.
If `heltec_t114_v2` is a board definition that only exists in your personal
Zephyr workspace (`$ZEPHYR_BASE` on your own machine) and was never part of
upstream Zephyr or committed into this repo, CI has no way to see it. Fix:
copy that board's definition folder into this repo under a top-level
`boards/` directory and commit it - the workflow auto-detects a `boards/`
folder and passes it to the build via `BOARD_ROOT`.
