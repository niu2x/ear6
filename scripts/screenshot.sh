#!/usr/bin/env bash
set -euo pipefail

CLI="$(dirname "$0")/../build/app/cli/ear6-cli"
FRAMES=120
TO_JPG=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    -f) FRAMES="$2"; shift 2 ;;
    -j) TO_JPG=true; shift ;;
    -*) echo "Unknown option: $1"; exit 1 ;;
    *) DIR="${1:-.}"; shift ;;
  esac
done
DIR="${DIR:-.}"

find "$DIR" -type f -iname '*.nes' -print0 | while IFS= read -r -d '' f; do
  out="${f%.*}.ppm"
  echo "$(basename "$f") -> ${out##*/}"
  timeout 30 "$CLI" screenshot "$f" -o "$out" -f "$FRAMES" &>/dev/null || { echo "  FAIL/TIMEOUT"; continue; }
  if $TO_JPG; then
    convert "$out" "${out%.*}.jpg" && rm "$out" && echo "  -> ${out##*/} -> ${out%.*}.jpg"
  fi
done
echo "DONE"
