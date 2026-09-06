#!/usr/bin/env bash
# Clone and build galette into app/pld_tests/.cache/ (gitignored).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
CACHE="$ROOT/.cache"
SRC="$CACHE/galette-src"
BIN="$CACHE/galette"
REPO_URL="${GALETTE_REPO:-https://github.com/simon-frankau/galette.git}"
REPO_REF="${GALETTE_REF:-master}"

if [[ -x "$BIN" ]]; then
  echo "$BIN"
  exit 0
fi

mkdir -p "$CACHE"
if [[ ! -d "$SRC/.git" ]]; then
  rm -rf "$SRC"
  git clone --depth 1 --branch "$REPO_REF" "$REPO_URL" "$SRC"
fi

# Avoid sandbox CARGO_TARGET_DIR redirect when present.
export CARGO_TARGET_DIR="$CACHE/galette-target"
mkdir -p "$CARGO_TARGET_DIR"
(cd "$SRC" && cargo build --release)
cp "$CARGO_TARGET_DIR/release/galette" "$BIN"
chmod +x "$BIN"
echo "$BIN"
