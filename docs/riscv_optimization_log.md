# RISC-V Optimization Log

This file records each independently committed optimization, its validation, and
the measured result.  Dynamic instruction counts are collected from Spike with
`script/functional/benchmark_riscv_spike.py`; static counts are taken from the
generated RV64 assembly.  Speedup is `baseline / optimized`.

## 1. Constant folding and load/store address folding

### Change

- Fold immediate-fed RV64 integer expressions after instruction selection.
- Fold `addi address, base, immediate` into the signed 12-bit offset of RV64
  integer and floating-point loads/stores.
- Let machine DCE remove the now-unused address arithmetic.

### Validation

- Clean compiler build: passed.
- GNU RV64 assembler/linker validation for all 60
  `testcases/riscv_finalperformance` programs: 60/60 passed.
- Spike output validation:
  - `testcases/functional/96_matrix_add.sy`: passed.
  - `testcases/functional/26_while_test1.sy`: passed.

### Result

| Scope | Baseline | Optimized | Reduction | Speedup |
|---|---:|---:|---:|---:|
| 60 RISC-V final-performance cases, static instructions | 31,314 | 27,068 | 13.56% | 1.157x |
| `96_matrix_add`, dynamic instructions | 345 | 315 | 8.70% | 1.095x |
| `96_matrix_add`, static instructions | 183 | 173 | 5.46% | 1.058x |
| `01_mm1` matrix kernel, static instructions | 526 | 400 | 23.95% | 1.315x |

The 60-case suite's small `li` count fell from 2,638 to 1,323 (49.85%).
`26_while_test1` remained at 160 dynamic instructions, which is the expected
neutral result for code without foldable address chains.

## 2. Interprocedural no-alias and memory-write summaries

### Change

- Prove pointer-parameter pairs as no-alias when every reachable direct call
  supplies distinct local/global allocations.
- Propagate no-alias facts through wrapper functions to a fixed point without
  allowing recursive call cycles to prove themselves.
- Compute per-function parameter-write summaries and propagate them through the
  call graph, allowing LICM/GVN/DSE to distinguish read-only calls and writes
  through unrelated pointer parameters.
- Keep library calls, global/unknown writes, and mixed aliasing call sites
  conservative.

### Validation

- Compiler build: passed.
- GNU RV64 compile/assemble validation:
  - `testcases/riscv_finalperformance`: 60/60 passed.
  - Existing `testcases/functional`: 99/100 passed; `64_calculator.sy` is an
    unchanged pre-existing frontend compile failure.
  - Both new alias-analysis regression tests: passed.
- Spike output validation: 7/7 selected array/matrix and alias regression
  cases passed, including a same-array counterexample that must remain MayAlias.
- `01_mm1` differential run produced identical P1/P2 output. Its output under
  Spike/newlib differs from the repository's expected file in both versions,
  so this case is used only for a like-for-like instruction comparison.

### Result

| Scope | P1 baseline | Optimized | Reduction | Stage speedup |
|---|---:|---:|---:|---:|
| `01_mm1`, dynamic instructions | 49,450 | 43,030 | 12.98% | 1.149x |
| `01_mm1`, static instructions | 400 | 391 | 2.25% | 1.023x |
| NoAlias/write-summary regression, dynamic instructions | 59 | 41 | 30.51% | 1.439x |
| 60 RISC-V final-performance cases, static instructions | 27,068 | 26,910 | 0.58% | 1.006x |

Relative to the original compiler, the 60-case static total is now down from
31,314 to 26,910 (14.06%, cumulative 1.164x).
