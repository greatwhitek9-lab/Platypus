#!/usr/bin/env bash
set -euo pipefail

chmod +x install_naughty_platypus.sh \
         scripts/build_naughty_platypus.sh \
         scripts/flash_naughty_platypus.sh \
         scripts/verify_naughty_platypus.sh \
         tools/naughty_platypus_host.py 2>/dev/null || true

echo "[+] Safe cipher patch files are in place."
echo
echo "Verify:"
echo "  grep -RIn 'np_safe_\\|NP_STATUS_IMPLEMENTED' firmware/naughty-platypus/src/tool_registry.c firmware/naughty-platypus/src/restricted_stubs.c"
echo
echo "Build:"
echo "  ./scripts/build_naughty_platypus.sh"
echo
echo "Commit/push:"
echo "  git add ."
echo "  git commit -m 'Apply safe Naughty Platypus cipher edits'"
echo "  git push origin naughty-platypus"
