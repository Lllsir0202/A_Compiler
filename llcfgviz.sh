#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 <input.ll> [output_dir]"
  exit 1
fi

INPUT_LL=$1
OUTPUT_DIR=${2:-cfgs}

if [[ ! -f "$INPUT_LL" ]]; then
  echo "Error: input file '$INPUT_LL' not found"
  exit 1
fi

if ! command -v opt >/dev/null 2>&1; then
  echo "Error: 'opt' not found in PATH"
  exit 1
fi

mkdir -p "$OUTPUT_DIR"

TMP_DIR=$(mktemp -d)
cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

INPUT_ABS=$(realpath "$INPUT_LL")
OUTPUT_ABS=$(realpath "$OUTPUT_DIR")

pushd "$TMP_DIR" >/dev/null
opt -passes=dot-cfg -disable-output "$INPUT_ABS"
shopt -s nullglob
DOT_FILES=(.*.dot)
if [[ ${#DOT_FILES[@]} -eq 0 ]]; then
  echo "No CFG dot files were generated"
  exit 1
fi

for dot_file in "${DOT_FILES[@]}"; do
  mv "$dot_file" "$OUTPUT_ABS/"
done
popd >/dev/null

echo "Exported ${#DOT_FILES[@]} CFG dot file(s) to $OUTPUT_DIR"

if command -v dot >/dev/null 2>&1; then
  png_count=0
  for dot_file in "$OUTPUT_ABS"/.*.dot; do
    [[ -e "$dot_file" ]] || continue
    png_file="${dot_file%.dot}.png"
    dot -Tpng -Gdpi=400 "$dot_file" -o "$png_file"
    ((png_count += 1))
  done
  echo "Rendered ${png_count} PNG file(s) to $OUTPUT_DIR"
else
  echo "Warning: 'dot' not found in PATH, skipped PNG rendering"
fi
