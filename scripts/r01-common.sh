#!/usr/bin/env bash
# Shared helpers for root ./studio ./emu ./sim wrappers.
# shellcheck shell=bash

r01_repo_root() {
  cd "$(dirname "${BASH_SOURCE[0]}")" && pwd
}

r01_die() {
  echo "error: $*" >&2
  exit 1
}

r01_usage_die() {
  echo "$*" >&2
  exit 2
}

# Ensure cmake build dir exists under $1 (project dir).
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
