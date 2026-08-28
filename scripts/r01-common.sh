#!/usr/bin/env bash
# Shared helpers for Retr01 repo scripts.
# shellcheck shell=bash

r01_repo_root() {
  cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd
}

if [[ -z "${R01_ROOT:-}" ]]; then
  R01_ROOT="$(r01_repo_root)"
fi
export R01_ROOT
: "${R01_ROM_DIR:=$R01_ROOT/rom}"
export R01_ROM_DIR
R01_DEFAULT_CART="$R01_ROM_DIR/test.retr01"
R01_DEFAULT_PROJECT="$R01_ROM_DIR/test.r01proj"
export R01_DEFAULT_CART R01_DEFAULT_PROJECT

r01_die() {
  echo "error: $*" >&2
  exit 1
}

r01_usage_die() {
  echo "$*" >&2
  exit 2
}

r01_resolve_path() {
  local p="$1"
  if [[ "$p" == /* ]]; then
    printf '%s\n' "$p"
    return
  fi
  if [[ -f "$R01_ROOT/$p" ]]; then
    printf '%s\n' "$R01_ROOT/$p"
    return
  fi
  if [[ -f "$p" ]]; then
    printf '%s\n' "$(cd "$(dirname "$p")" && pwd)/$(basename "$p")"
    return
  fi
  printf '%s\n' "$R01_ROOT/$p"
}

r01_proj_dir() {
  local name="$1"
  printf '%s' "$R01_ROOT/$name"
}

r01_ensure_cmake() {
  local proj="$1"
  if [[ ! -f "$proj/build/CMakeCache.txt" ]]; then
    cmake -B "$proj/build" -DCMAKE_BUILD_TYPE=Release
  fi
}

r01_build() {
  local proj="$1"
  shift
  r01_ensure_cmake "$proj"
  cmake --build "$proj/build" "$@"
}

r01_ctest() {
  local proj="$1"
  shift
  ctest --test-dir "$proj/build" --output-on-failure "$@"
}
