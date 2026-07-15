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

## 3. Bounded memoization for safe direct recursion

### Change

- Detect two-integer recursive functions with multiple recursive call sites,
  read-only bodies, and no calls other than direct self-recursion.
- Require exactly one non-recursive, straight-line call from `main`, making a
  process-lifetime zero-initialized cache safe even when the function reads
  arrays initialized immediately before the call.
- Rename the original body to a private implementation and route both the
  top-level and recursive calls through a bounded memoization wrapper.
- Cache states in `[0, 64) x [0, 256)`; unknown out-of-range states safely call
  the original implementation, while statically known out-of-range calls are
  left entirely unchanged.

### Validation

- Compiler build: passed.
- GNU RV64 compile/assemble validation for all 60
  `testcases/riscv_finalperformance` programs: 60/60 passed.
- Functional RV64 compile/assemble validation: 103/104 passed. The only failure
  is the unchanged frontend failure in `64_calculator.sy`; the slow
  `82_long_func.sy` also passed when allowed its normal >15 second compile time.
- Spike output validation:
  - Fixed-data recursive knapsack: passed with output 1080.
  - Known out-of-range state: passed with output 89 and remained byte-for-byte
    equivalent in static/dynamic instruction counts to P2.
- Native execution of the three official recursive cases preserved their
  outputs: 52, 117, and 88. This is an auxiliary correctness check because the
  existing Spike/newlib baseline follows an incorrect short path for their
  `getarray` input layout.

### Result

| Scope | P2 baseline | Optimized | Reduction | Stage speedup |
|---|---:|---:|---:|---:|
| Fixed-data knapsack, Spike dynamic instructions | 2,302,930,866 | 258,233 | 99.9888% | 8,918.035x |
| Fixed-data knapsack, Spike elapsed time | 37.436 s | 0.026 s | 99.93% | 1,452.3x |
| Known out-of-range regression, dynamic instructions | 1,644 | 1,644 | 0% | 1.000x |
| 60 RISC-V final-performance cases, static instructions | 26,910 | 27,036 | -0.47% | 0.995x |

The static increase is the bounded lookup wrapper; it replaces exponential
dynamic work on the three matching recursive programs. Native timer regions for
those official cases changed from 377,993/332,570/255,265 microseconds to
10/6/11 microseconds, while preserving output. The optimized Spike instruction
figure is the median of five fresh compiler/run samples (258,233 in four runs;
261,775 in one); elapsed time is the corresponding rounded median.

## 4. Integer reassociation and late dead-code cleanup

### Change

- Normalize `x - C` to the canonical wrapped-integer form `x + (-C)`.
- Reassociate one-level integer add/multiply trees to reuse the earliest
  dominating pair and combine adjacent constants with explicit 32-bit wrapping.
- Factor single-use `(a*c) + (b*c)` trees into `(a+b)*c`; this targets the
  repeated nonlinear polynomial in three final-performance programs.
- Run a distinct post-reassociation ADCE after LICM and scalar promotion, where
  it can finally remove expression/address nodes made dead by those late passes.

### Validation

- Compiler build: passed.
- GNU RV64 compile/assemble validation:
  - `testcases/riscv_finalperformance`: 60/60 passed.
  - Functional cases: 104/105 passed; the only failure remains the unchanged
    frontend failure in `64_calculator.sy`. `82_long_func.sy` passed with its
    normal >15 second compile time.
- Spike output validation: 10/10 selected alias, matrix, memoization,
  out-of-range, and reassociation cases passed.
- Native execution of the three recursive official cases still produced
  52/117/88 after the late cleanup.

### Result

| Scope | P3 baseline | Optimized | Reduction | Stage speedup |
|---|---:|---:|---:|---:|
| Reassociation regression, dynamic instructions | 23,270 | 22,145 | 4.83% | 1.051x |
| NoAlias/write-summary regression, dynamic instructions | 41 | 32 | 21.95% | 1.281x |
| `2025-MGB-26`, differential dynamic instructions | 346,579 | 338,387 | 2.36% | 1.024x |
| 60 RISC-V final-performance cases, static instructions | 27,036 | 26,308 | 2.69% | 1.028x |

`2025-MGB-26` has the same pre-existing Spike/newlib output mismatch in both
versions, so that row is a differential instruction comparison rather than an
output-conformance result. Relative to the original compiler, the 60-case
static total is now down from 31,314 to 26,308 (15.99%, cumulative 1.190x).

## 5. Loop-aware block placement and branch fallthrough

### Change

- Build greedy machine-CFG traces from the entry block, preferring the true
  successor (normally a loop body or hot `then` path) as the next emitted block.
- Rewrite `bcond next; j other` as `b!cond other` plus fallthrough.
- Continue removing unconditional jumps to the immediately following block.
- Run placement once and branch folding to a machine-pass fixpoint, preserving
  the CFG while avoiding layout oscillation.

The proposed semantic loop interchange/distribution was audited but not applied:
the primary `01_mm1` kernel is already in cache-friendly `k-i-j` order, while a
general interchange without array-dependence proof would be unsafe. The trace
layout still improves loop execution by making body edges fall through.

### Validation

- Compiler build: passed.
- GNU RV64 compile/assemble validation:
  - `testcases/riscv_finalperformance`: 60/60 passed.
  - Functional cases: 104/105 passed; only the unchanged `64_calculator.sy`
    frontend failure remains. `82_long_func.sy` passed separately.
- Spike output validation: 10/10 selected functional cases passed.
- All 1,110 remaining static condition-plus-jump pairs in the 60-case suite were
  eliminated; 231 pairs that could not use the original order became foldable
  after trace placement.

### Result

| Scope | P4 baseline | Optimized | Reduction | Stage speedup |
|---|---:|---:|---:|---:|
| 10 validated cases, dynamic instructions | 284,223 | 275,520 | 3.06% | 1.032x |
| `01_mm1`, differential dynamic instructions | 43,030 | 41,681 | 3.14% | 1.032x |
| Fixed-data recursive knapsack, dynamic instructions | 258,233 | 249,616 | 3.34% | 1.035x |
| 60 RISC-V final-performance cases, static instructions | 26,308 | 24,908 | 5.32% | 1.056x |

`01_mm1` again uses identical P4/P5 output under Spike but retains the same
pre-existing mismatch against the repository expected file. Relative to the
original compiler, the 60-case static total is down from 31,314 to 24,908
(20.46%, cumulative 1.257x).

## Build portability follow-up

- Added the direct `<stdexcept>` dependency to `recursivememo.h`, which uses
  `std::invalid_argument` in its inline constructor.
- Added the same direct dependency to `reassociate.h`; an audit confirmed that
  it was the only other pass header using `std::invalid_argument` without
  directly including `<stdexcept>`.
- This prevents isolated or differently ordered builds from depending on a
  transitive standard-library include.
