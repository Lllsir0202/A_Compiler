#!/usr/bin/env bash
# RVV correctness harness: compile each .sy with -rvv, assemble for rv64gcv, run
# under an RVV-capable QEMU, and diff stdout+exit-code against the .out golden.
#   rvv_check.sh <testdir> [opt=-O1] [vlen=256]
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR="${1:?testdir}"
OPT="${2:--O1}"
VLEN="${3:-256}"
QEMU="${QEMU:-/tmp/qemu-8.2.2/build/qemu-riscv64}"
COMPILER="${COMPILER:-$ROOT/bin/compiler}"
GCC=riscv64-linux-gnu-gcc

WORK="$(mktemp -d /tmp/rvvcheck.XXXXXX)"
total=0; passed=0; failed=0
for sy in "$DIR"/*.sy; do
  base=$(basename "$sy" .sy); exp="$DIR/$base.out"; in="$DIR/$base.in"
  [ -f "$exp" ] || continue
  s="$WORK/$base.s"; elf="$WORK/$base.elf"; got="$WORK/$base.got"
  "$COMPILER" "$sy" -o "$s" -S -riscv -rvv $OPT >/dev/null 2>&1 || { echo "$base COMPILE_FAIL"; failed=$((failed+1)); continue; }
  $GCC -march=rv64gcv -static "$s" "$ROOT/libs/sylib.c" -I "$ROOT/libs" -o "$elf" >/dev/null 2>&1 || { echo "$base ASM_FAIL"; failed=$((failed+1)); continue; }
  if [ -f "$in" ]; then timeout 60 $QEMU -cpu rv64,v=true,vlen=$VLEN "$elf" < "$in" > "$got" 2>/dev/null
  else timeout 60 $QEMU -cpu rv64,v=true,vlen=$VLEN "$elf" > "$got" 2>/dev/null; fi
  code=$?
  act="$WORK/$base.act"; cp "$got" "$act"
  { [ -s "$act" ] && [ -n "$(tail -c1 "$act")" ] && printf '\n' >> "$act"; }
  printf '%s\n' "$code" >> "$act"
  if diff -q <(sed -e 's/[[:space:]]*$//' "$act") <(sed -e 's/[[:space:]]*$//' "$exp") >/dev/null 2>&1; then
    passed=$((passed+1))
  else
    failed=$((failed+1)); echo "$base FAIL"
  fi
  total=$((total+1))
done
echo "# RVV: dir=$(basename "$DIR") opt=$OPT vlen=$VLEN pass=$passed fail=$failed total=$total"
