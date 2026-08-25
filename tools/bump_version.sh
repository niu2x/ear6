#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cmake_file="$root/CMakeLists.txt"

cur=$(sed -n 's/^project(Ear6 VERSION \([0-9.]*\) .*/\1/p' "$cmake_file")
if [[ -z "$cur" ]]; then
    echo "ERROR: cannot find current version in $cmake_file" >&2
    exit 1
fi

IFS='.' read -r major minor patch <<< "$cur"

if [[ $# -eq 0 ]]; then
    echo "Current version: $cur"
    echo "Usage: $0 <major|minor|patch>"
    echo "  $0 patch   -> ${major}.${minor}.$((patch+1))"
    echo "  $0 minor   -> ${major}.$((minor+1)).0"
    echo "  $0 major   -> $((major+1)).0.0"
    exit 0
fi

case "$1" in
    major) new="$((major+1)).0.0" ;;
    minor) new="${major}.$((minor+1)).0" ;;
    patch) new="${major}.${minor}.$((patch+1))" ;;
    *)     echo "ERROR: unknown part '$1', use major|minor|patch" >&2; exit 1 ;;
esac

sed -i '' "s/^project(Ear6 VERSION ${cur} /project(Ear6 VERSION ${new} /" "$cmake_file"
echo "$cur -> $new"
