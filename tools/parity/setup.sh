#!/usr/bin/env bash
# Creates the structural-parity venv under $IWD2R_HOME/parity-venv (user-local,
# no sudo) with the exact pins from requirements.txt (tools/parity/run.py's
# exe-based columns need pefile + capstone; the lints run on bare python3).
# Idempotent: an existing venv that already satisfies the pins is left alone.
#
# This system's python3 has no pip/ensurepip (python3-venv ships without it),
# so the venv is created --without-pip and pip itself is bootstrapped from the
# official per-version get-pip installer, pinned by version.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="${IWD2R_HOME:-$HOME/.local/share/iwd2r}"
venv="$root/parity-venv"
pip_pin=24.0

satisfied() {
  [ -x "$venv/bin/python" ] || return 1
  "$venv/bin/python" -I - "$here/requirements.txt" <<'EOF' 2>/dev/null
import importlib.metadata as md
import sys
try:
    for line in open(sys.argv[1]):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, _, ver = line.partition("==")
        if md.version(name.replace("-", "_")) != ver:
            sys.exit(1)
except Exception:
    sys.exit(1)
sys.exit(0)
EOF
}

if satisfied; then
  echo "parity venv already satisfies $(grep -v '^#' "$here/requirements.txt" | tr -s ' \n' ' ')"
  exit 0
fi

mkdir -p "$root"

if [ ! -x "$venv/bin/python" ]; then
  echo "creating venv $venv ($(python3 -I -V 2>&1))"
  python3 -I -m venv --without-pip "$venv"
fi

if ! "$venv/bin/python" -I -m pip --version >/dev/null 2>&1; then
  tmp=$(mktemp -d "$root/.getpip.XXXXXX")
  trap 'rm -rf "$tmp"' EXIT
  # The rolling official installer (per-version /pip/<v>/ paths exist only for
  # EOL pythons); the pip it installs is pinned below.
  curl -fsSL --retry 3 -o "$tmp/get-pip.py" "https://bootstrap.pypa.io/get-pip.py"
  "$venv/bin/python" -I "$tmp/get-pip.py" "pip==$pip_pin"
fi

"$venv/bin/pip" install -q -r "$here/requirements.txt"

echo
echo "Install root : $root"
echo "parity venv  : $venv"
"$venv/bin/pip" freeze 2>/dev/null | grep -Ei '^(pefile|capstone|pip)==' | sed 's/^/  /'
echo "Gate entry   : $venv/bin/python tools/parity/run.py --base <ref>"
