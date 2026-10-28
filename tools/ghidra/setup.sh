#!/usr/bin/env bash
# Installs Ghidra, Node.js and REA under ~/.local/share/iwd2r (user-local, no sudo).
# Idempotent: anything already present at the pinned version is left alone.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="${IWD2R_HOME:-$HOME/.local/share/iwd2r}"
versions="$here/versions.json"

die() { echo "setup.sh: $*" >&2; exit 1; }
field() { python3 -I -c 'import json,sys; d=json.load(open(sys.argv[1])); print(d[sys.argv[2]][sys.argv[3]])' "$versions" "$1" "$2"; }

# Java 21 (this script has no sudo).
if ! java -version 2>&1 | head -1 | grep -q '"21\.'; then
  echo "Java 21 is required. Install it with:" >&2
  echo "  install Java 21 and unzip (e.g. apt-get install -y openjdk-21-jdk-headless unzip)" >&2
  exit 1
fi
command -v unzip >/dev/null || { echo "unzip missing. Install it (e.g. apt-get install -y openjdk-21-jdk-headless unzip)" >&2; exit 1; }

# The JDK a Ghidra run exports as JAVA_HOME: $JAVA_HOME when it is a
# directory, else the Debian apt path when that exists, else the java on PATH
# (two levels above bin/java). Distros disagree on the path; only printed here.
if [ -n "${JAVA_HOME:-}" ] && [ -d "$JAVA_HOME" ]; then
  jdk=$JAVA_HOME
elif [ -d /usr/lib/jvm/java-21-openjdk-amd64 ]; then
  jdk=/usr/lib/jvm/java-21-openjdk-amd64
else
  jdk=$(python3 -I -c 'import os, shutil
p = shutil.which("java")
print(os.path.dirname(os.path.dirname(os.path.realpath(p))) if p else "")')
fi

g_url=$(field ghidra url); g_sha=$(field ghidra sha256); g_tag=$(field ghidra tag)
n_url=$(field node url);   n_sha=$(field node sha256);   n_ver=$(field node version)
r_pkg=$(field rea package); r_ver=$(field rea version);  r_int=$(field rea integrity)

mkdir -p "$root"
tmp=$(mktemp -d "$root/.dl.XXXXXX")
trap 'rm -rf "$tmp"' EXIT

fetch() { # url sha256 dest
  curl -fsSL --retry 3 -o "$3" "$1"
  local got; got=$(sha256sum "$3" | cut -d' ' -f1)
  [ "$got" = "$2" ] || die "SHA-256 mismatch for $1: expected $2, got $got"
  echo "verified sha256 $2  $(basename "$3")"
}

# Ghidra
if [ "$(cat "$root/ghidra/.iwd2r-tag" 2>/dev/null)" = "$g_tag" ] && [ -x "$root/ghidra/support/analyzeHeadless" ]; then
  echo "Ghidra $g_tag already present"
else
  fetch "$g_url" "$g_sha" "$tmp/ghidra.zip"
  mkdir "$tmp/g"; unzip -q "$tmp/ghidra.zip" -d "$tmp/g"
  rm -rf "$root/ghidra"; mv "$tmp/g"/ghidra_* "$root/ghidra"
  echo "$g_tag" > "$root/ghidra/.iwd2r-tag"
fi

# Node
if [ -x "$root/node/bin/node" ] && [ "$("$root/node/bin/node" --version)" = "v$n_ver" ]; then
  echo "Node v$n_ver already present"
else
  fetch "$n_url" "$n_sha" "$tmp/node.tar.xz"
  mkdir "$tmp/n"; tar -xJf "$tmp/node.tar.xz" -C "$tmp/n"
  rm -rf "$root/node"; mv "$tmp/n"/node-* "$root/node"
fi
export PATH="$root/node/bin:$PATH"

# REA (published npm package, pinned; npm checks the registry integrity hash)
rea_pkgjson="$root/rea/node_modules/$r_pkg/package.json"
if [ -f "$rea_pkgjson" ] && grep -q "\"version\": \"$r_ver\"" "$rea_pkgjson"; then
  echo "$r_pkg $r_ver already present"
else
  if [ -n "$r_int" ]; then
    got=$(npm view "$r_pkg@$r_ver" dist.integrity)
    [ "$got" = "$r_int" ] || die "npm integrity mismatch for $r_pkg@$r_ver: expected $r_int, got $got"
    echo "verified npm integrity $r_int"
  fi
  mkdir -p "$root/rea"
  [ -f "$root/rea/package.json" ] || echo '{"name":"iwd2r-rea","private":true}' > "$root/rea/package.json"
  npm install --prefix "$root/rea" --save-exact --no-audit --no-fund "$r_pkg@$r_ver"
fi

echo
echo "Install root : $root"
echo "Ghidra       : $root/ghidra ($g_tag)  GHIDRA_INSTALL_DIR"
echo "Node         : $root/node/bin/node (v$n_ver)"
echo "REA          : $root/rea/node_modules/$r_pkg ($r_ver)"
echo "JDK          : $jdk  JAVA_HOME"
