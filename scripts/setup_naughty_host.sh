#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENV="${ROOT}/.venv-naughty"

python3 -m venv "${VENV}"
"${VENV}/bin/python" -m pip install --upgrade pip
"${VENV}/bin/python" -m pip install -r "${ROOT}/tools/requirements-naughty-platypus.txt"

cat <<EOF
Naughty Platypus host environment is ready.

Activate it:
  source "${VENV}/bin/activate"

Run the TUI:
  python3 "${ROOT}/tools/naughty_platypus_host.py" --port /dev/ttyACM0 --tui
EOF
