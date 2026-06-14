#!/usr/bin/env bash

# compare between O0 and O1
# ./script/functional/runall_llvm_benchmark.sh x86 1 -O0
# ./script/functional/runall_llvm_benchmark.sh x86 1 -O1


# benchmark for specific dataset
# cd script/functional
# python3 benchmark_llvm.py ../../testcases/functional --rounds 1 --opt=-O0

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
ARCH=${1:-x86}
ROUNDS=${2:-1}
OPT=${3:--O0}

python3 "$SCRIPT_DIR/benchmark_llvm.py" \
  "$REPO_ROOT/testcases/functional" \
  "$REPO_ROOT/testcases/h_functional" \
  "$REPO_ROOT/testcases/riscv_performance" \
#   "$REPO_ROOT/testcases/riscv_finalperformance" \
#   "$REPO_ROOT/testcases/arm_performance" \
#   "$REPO_ROOT/testcases/arm_finalperformance" \
  --arch "$ARCH" \
  --rounds "$ROUNDS" \
  --opt="$OPT" \
  --output-root "$REPO_ROOT/testoutputs/benchmark_llvm"
