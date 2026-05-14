#!/usr/bin/env bash
set -euo pipefail

CLI="$(dirname "$0")/../build/app/cli/ear6-cli"
DIR="${1:-.}"

find "$DIR" -type f -iname '*.nes' -print0 | while IFS= read -r -d '' f; do
  out="${f%.*}.ppm"
  echo "$(basename "$f") -> ${out##*/}"
  timeout 2 "$CLI" screenshot "$f" -o "$out" -f 120 &>/dev/null || echo "  FAIL/TIMEOUT"
done
echo "DONE"
