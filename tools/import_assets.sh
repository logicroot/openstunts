#!/usr/bin/env bash
#
# Copy the original Stunts data files into ./assets so the game is
# self-contained and no original installation is needed at run time.
#
# Only *data* files are copied. The DOS program files are deliberately left
# behind: the port reimplements the game code, so shipping the original
# executables would not help it run and would just be a copy of the DOS game.
# Skipped extensions:
#
#   *.EXE  DOS executables            (ST.COM, LOAD.EXE, SETUP.EXE, ...)
#   *.COM  DOS executables            (STUNTS.COM, ...)
#   *.DRV  loadable driver binaries   (AD15.DRV, MT15.DRV, PC15.DRV, TD15.DRV)
#
# The .DRV files are COM-style programs too (they start with a near JMP), they
# only masquerade as data to `file`.
#
# Usage: tools/import_assets.sh [STUNTS_DIR]     (default: ~/stunts)

set -euo pipefail

src="${1:-$HOME/stunts}"
root="$(cd "$(dirname "$0")/.." && pwd)"
dst="$root/assets"

if [ ! -d "$src" ]; then
  echo "import_assets: source directory '$src' not found" >&2
  exit 1
fi

mkdir -p "$dst"

copied=0
skipped=0
for f in "$src"/*; do
  [ -f "$f" ] || continue
  name="$(basename "$f")"
  case "${name^^}" in
    *.EXE | *.COM | *.DRV)
      echo "  skip (program) $name"
      skipped=$((skipped + 1))
      continue
      ;;
  esac
  # A zero-byte file is never a real asset (e.g. a stray editor artefact).
  if [ ! -s "$f" ]; then
    echo "  skip (empty)   $name"
    skipped=$((skipped + 1))
    continue
  fi
  cp -f "$f" "$dst/$name"
  copied=$((copied + 1))
done

echo "import_assets: copied $copied data file(s) to $dst (skipped $skipped)"
