#!/usr/bin/env bash
# Fetch a pinned llvm-mos-sdk into tools/llvm-mos (not committed).
# Usage: ./scripts/fetch-llvm-mos.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="${LLVM_MOS_VERSION:-v23.2.0}"
DEST="$ROOT/tools/llvm-mos"
URL="https://github.com/llvm-mos/llvm-mos-sdk/releases/download/${VER}/llvm-mos-linux.tar.xz"

if [ -x "$DEST/bin/mos-common-clang" ]; then
  echo "llvm-mos already at $DEST"
  exit 0
fi

mkdir -p "$ROOT/tools"
TMP="$ROOT/tools/llvm-mos-linux.tar.xz"
echo "fetching llvm-mos-sdk ${VER}"
curl -L --fail -o "$TMP" "$URL"
rm -rf "$DEST"
mkdir -p "$DEST"
tar --no-same-owner -xJf "$TMP" -C "$DEST" --strip-components=1
rm -f "$TMP"
"$DEST/bin/mos-common-clang" --version | head -1
echo "installed $DEST"
