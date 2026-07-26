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

## Interprocedural pointer no-alias refinement

The BASEIR alias analysis now uses the closed-world, direct-call nature of SysY
programs to prove pairs of pointer parameters distinct when every call site
passes already-distinct storage. Facts propagate through wrappers to a fixed
point, while unseeded recursive cycles and any may-alias call site remain
conservative.

This refinement is layered on the existing SCEV-aware pointer-phi analysis and
the richer `FunctionSideEffectPass` summaries. The older standalone
parameter-write summary from the source change was intentionally not imported:
the existing per-argument read/write effects and `CallCanNameObject` handling
are both more precise and correctly model a local pointer passed to the current
call.

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

## 6. Multidimensional pointer induction variables

### Change

- Generalize getptr strength reduction from an affine last index to exactly one
  affine index in any dimension of a scalar-element access.
- Derive the scalar-element stride from recursive array types, accounting for
  the different first-index semantics of array objects and array parameters.
- Scale both the loop step and unrolled-family offsets by that dimension stride,
  turning accesses such as `a[k][j]` into a walking pointer plus fixed offsets.
- Restrict non-last-dimension reduction to innermost accesses of rank four or
  less. This avoids keeping many wide-stride pointers live across subloops and
  prevents register-spill regressions on pathological high-rank arrays.

### Validation

- Clean incremental compiler build: passed.
- Compile and GNU-assemble all 60 RISC-V final-performance programs: 60/60
  passed.
- Compile and Clang-assemble all 60 AArch64 final-performance programs: 60/60
  passed.
- Functional plus hidden-functional compile/assemble on both targets: 145/146
  passed; the only failure on each remains the pre-existing frontend failure in
  `64_calculator.sy`.
- Spike output validation passed for matrix add/subtract/multiply/transpose,
  the 19-dimensional array stress case, and a dedicated runtime-bounded column
  walk.

### Result

| Scope | Baseline | Optimized | Reduction | Speedup |
|---|---:|---:|---:|---:|
| Runtime-bounded 2D column walk, dynamic instructions | 4,514 | 3,439 | 23.81% | 1.313x |
| `many_mat_cal` hot unrolled reduction group, instructions | 67 | 51 | 23.88% | 1.314x |
| `matmul1`, static instructions | 555 | 490 | 11.71% | 1.133x |
| 60 RISC-V final-performance cases, static instructions | 24,925 | 24,550 | 1.50% | 1.015x |

Sixteen of the 18 statically changed final-performance programs became smaller;
the two increases were two instructions each. The high-rank stress case stayed
effectively neutral at 9,989,494 versus 9,989,493 dynamic instructions, showing
that the cost model avoids the register-pressure regression found during
development.

## 7. Four-column unroll-and-jam for in-place matrix products

### Change

- Recognize the strict `A[i][j] = sum_k C[i][k] * A[k][j]` structured-loop
  shape with two distinct global two-dimensional arrays, unit induction steps,
  a single zero-initialized reduction, and no additional memory effects.
- Split the column loop into a four-column main loop plus the original scalar
  remainder. The jammed reduction loads `C[i][k]` once and updates four sums
  from the contiguous `A[k][j..j+3]` row segment.
- Keep the four reduction chains independent so the existing eight-way inner
  partial unroller exposes instruction-level parallelism without changing each
  column's arithmetic order.
- Test the main-loop boundary as `j + 3 < bound`; this avoids the signed
  underflow hazard of computing `bound - 3` for a runtime bound.
- Require distinct global-array definitions instead of accepting pointer
  parameters, so possible caller-side aliasing cannot invalidate the schedule.

### Validation

- Incremental compiler build and MLIR verification: passed.
- Functional plus hidden-functional plus final-performance compile/assemble on
  both RISC-V and AArch64: 206/207 passed; the only failure on each target is
  the unchanged frontend failure in `64_calculator.sy`.
- Native host execution: 146/147 functional plus hidden-functional cases
  passed with exact output; only `64_calculator.sy` failed to compile.
- The dedicated runtime-bound regression exercises both a four-column group
  and the scalar remainder; optimized RISC-V, unoptimized RISC-V, and optimized
  native execution produced identical output.
- Only the three `many_mat_cal` programs among the 60 final-performance cases
  receive the large transformed kernel; no unrelated matrix kernel matched.

### Result

| Scope | Baseline | Optimized | Reduction | Instruction speedup |
|---|---:|---:|---:|---:|
| Runtime-bound in-place dot-product regression, dynamic instructions | 3,050 | 2,178 | 28.59% | 1.400x |
| Same regression, static instructions | 229 | 489 | -113.54% | 0.468x |
| Each `many_mat_cal` program, static instructions | 550 | 798 | -45.09% | 0.689x |

The static increase is deliberate: four accumulator chains and their
eight-iteration widened reduction trade code size for row-contiguous cache-line
use. The submitted wall-clock measurements are therefore the decisive metric
for this stage; the local instruction regression confirms reduced loop/address
overhead but does not model the judge's cache timing.

## 8. Register-pressure-aware machine LICM

### Change

- Add a RISC-V machine-level loop-invariant code motion pass
  (`backend/targets/riscv/pass/machinelicm.{h,cpp}`), scheduled after local CSE
  and before machine DCE. It targets invariant code the middle-end LICM cannot
  see because it is created during instruction selection: the `slli`/`add`
  address chains for multidimensional array walks and materialized loop-bound
  constants.
- Build the pass's own CFG, dominator sets, and natural-loop detection from the
  pre-register-allocation block list (the backend does not populate CFG edges at
  this stage). Hoist only into a unique out-of-loop preheader that dominates the
  header, inserting before that block's terminator.
- Restrict hoisting to pure, cheap, single-definition opcodes whose operands are
  all loop-invariant (or already hoisted). Exclude `divw`/`remw`: trap-free on
  RISC-V but too expensive to speculate into a loop that may run zero times.
- Make hoisting register-pressure aware, which is the decisive design point.
  Multi-input arithmetic can let its now-loop-dead source operands be freed, so
  it is hoisted freely. A trivially rematerializable `li`/`la` frees no operand
  and adds one value live across the whole loop, so it is gated by two limits,
  the smaller winning per register class: a budget derived from the loop's
  estimated peak live-set versus the 25-GPR / 32-FPR file, and a small absolute
  cap. The cap is charged cumulatively against rematerializable values already
  in the preheader, so the pass manager's fixpoint re-runs cannot defeat it by
  hoisting a fresh batch each iteration.

### Validation

- Incremental compiler build: passed.
- Compile plus GNU-assemble of all 59 `riscv_performance` cases: 59/59 passed.
- Compile of the functional and hidden-functional suites: 151/152 passed; the
  only failure is the pre-existing frontend failure in `64_calculator.sy`, which
  fails identically on the pass-disabled baseline.
- Differential dynamic-instruction count under Spike, pass-enabled versus an
  otherwise identical pass-disabled build, across the 59-case performance suite.
  All four exact-output (`OK`) cases stayed `OK`; no status regressed.

### Result

| Scope | Baseline | Optimized | Reduction | Instruction speedup |
|---|---:|---:|---:|---:|
| Performance suite, total dynamic instructions | 7,081,046,428 | 7,050,083,785 | 0.44% | 1.0044x |
| `h-11-*` convolution/pooling kernels (each) | ~5.58M / 39.5M | ~4.32M / 30.5M | ~22.6% | ~1.29x |
| `h-2-*` kernels (each) | 284.6M / 279.3M / 409.8M | 278.4M / 273.1M / 400.8M | ~2.2% | ~1.022x |

The net gain is modest and concentrated in a few hot loops (the `h-11` and `h-2`
families dominate the reduction), with `if-combine` and `h-5` contributing
smaller wins. It is not a uniform improvement: `h-4-02` (+19%) and `transpose2`
(+5%) still regress from second-order register-allocation effects the pressure
estimate does not fully model, and several tiny-trip-count cases (`matmul*`,
`gameoflife-*`, `sl*`) show small static-overhead regressions where the
preheader setup is not amortized. The pressure model was added after measuring
that a pressure-blind version regressed `h-4-02` by +80% (+4.2M instructions)
via hot-loop spills; gating rematerializable hoists cut that to +19% while
preserving the `h-11`/`h-2` wins. This is the well-known trade-off of running
LICM before register allocation on a small register file: loop-body savings and
allocation cost are close in magnitude, and the pass is tuned to stay net
positive rather than to win every case.

## 9. Exact register-allocation liveness fixpoint and complete local CSE

### Change

- Fix graph-coloring liveness convergence to compare complete live-in/live-out
  sets. Equal set cardinality is not an unchanged dataflow fact: a loop can
  replace one live value with another while preserving the size. The previous
  shortcut could stop early, omit an interference edge, and let coalescing
  assign one physical register to simultaneously live values.
- Validate every completed RISC-V coloring before rewriting virtual registers:
  adjacent nodes must have different colors and coalesced nodes must agree with
  their representative. This converts future allocator invariant violations
  into a deterministic compiler failure instead of silently emitting wrong
  code.
- Apply the same dataflow fix to the AArch64 allocator, which shared the
  algorithmic defect.
- With correct interference information, restore standard complete local CSE
  inside a basic block. Cross-block CSE remains pressure-aware and requires the
  canonical value to be live-in, so the change does not extend arbitrary values
  through the dominator tree.
- Keep the post-Phi compatibility CSE/LICM pipeline for now. Removing it
  increased the 60-case instruction total by about 1.06%; it remains necessary
  until register allocation consumes machine-SSA Phi nodes directly.

### Validation

- RISC-V directed tests: 58/58 passed across `-O0` and `-O1`.
- Functional suites: 109/109 and 40/40 passed at both optimization levels.
- 2026 RISC-V functional suites: 100/100 and 40/40 passed at both optimization
  levels.
- 2026 RISC-V performance suite: 60/60 passed at both optimization levels.
- Historical RISC-V performance suites: 57/57 and 60/60 passed at `-O1`.
- No ARM runtime tests were run; the requested RISC-V-only policy was kept.

### Result

| Scope | M2 baseline | M3 | Change |
|---|---:|---:|---:|
| 2026 60-case total dynamic instructions | 20,635,470,697 | 20,599,209,245 | -0.176% |
| 2026 60-case geometric mean | 1.0000 | 0.9960 | -0.40% |
| `03_sort1` | 156,251,718 | 144,168,198 | -7.73% |
| `03_sort2` | 156,401,558 | 144,300,362 | -7.74% |
| `03_sort3` | 156,176,027 | 144,099,213 | -7.73% |

Ten alternating whole-suite wall-clock measurements produced medians of
8.490 s for M2 and 8.535 s for M3. The 0.045 s difference is smaller than the
observed run-to-run noise (individual runs ranged from 8.42 s to 8.90 s), so
wall-clock performance is treated as statistically unchanged rather than as a
measured regression or speedup.

## 10. Pressure-monotonic machine-SSA list scheduling

### Change

- Add a deterministic local list scheduler for RISC-V machine-SSA basic
  blocks. Calls, Phi nodes, terminators, comments and unknown side effects are
  pinned; each remaining straight-line region is scheduled independently.
- Build explicit register RAW/WAR/WAW dependencies. All loads and stores also
  participate in one conservative memory chain, preserving memory order without
  relying on incomplete machine-level alias analysis.
- Use a small target latency model for loads, integer and floating multiply,
  divide and conversions. Ready instructions whose inputs are available are
  preferred, while critical-path height, operand release and early loads provide
  deterministic tie-breaking.
- Make scheduling register-pressure monotonic. A candidate order is rejected if
  it raises GPR/FPR peak pressure, total live-range length, or introduces any
  new pair of overlapping virtual-register live ranges. This is stricter than a
  numeric pressure budget: it guarantees the local interference relation is a
  subset of the original schedule.
- The interference condition was added after measuring that a latency-only
  scheduler increased the 60-case geometric-mean instruction count by 2.21%
  through spills (`huffman` about +7.2%). Peak/length gating reduced the total
  regression but still left `crypto` about +1.79%; preserving the complete
  overlap relation removed both classes of regression without testcase-specific
  rules.

### Validation

- RISC-V directed tests: 59/59 passed across `-O0` and `-O1`, including a new
  dependency regression with loads, independent arithmetic, a store and call
  boundaries.
- Functional and hidden-functional suites: 109/109 and 40/40 passed at `-O1`.
- 2026 RISC-V functional suites: 100/100 and 40/40 passed at `-O1`.
- 2026 RISC-V performance suite: 60/60 passed at `-O1`.
- Historical RISC-V performance suites: 57/57 and 60/60 passed at `-O1`.
- No ARM runtime tests were run.

### Result

| Scope | M3 | M4 scheduler | Change |
|---|---:|---:|---:|
| 2026 60-case total dynamic instructions | 20,599,209,245 | 20,599,099,374 | -0.0005% |
| `sl1` | 208,641,473 | 208,602,468 | -0.0187% |
| `sl2` | 409,740,372 | 409,679,117 | -0.0149% |
| `sl3` | 25,927,700 | 25,918,195 | -0.0367% |

Six alternating whole-suite wall-clock measurements produced medians of
8.315 s for M3 and 8.300 s for M4 (about 0.18% faster). This remains within
local QEMU run-to-run noise. QEMU user-mode execution does not model the target
BOOM pipeline's load-use and functional-unit latency accurately, so the local
result establishes absence of a measurable regression; platform A/B timing is
needed to quantify the scheduler's latency benefit.

## 11. Profitable post-register-allocation scheduling

### Change

- Run the dependence scheduler a second time after graph coloring and before
  frame finalization. With physical registers fixed, latency-oriented reordering
  cannot create new spills; physical RAW/WAR/WAW edges preserve the allocation.
- Canonicalize physical dependency keys by register class and architectural
  register ID, deliberately ignoring the operand's Word/QWord view. Treating
  those views as separate pointer-identified values initially caused widespread
  wrong code because, for example, both views still name `a0`.
- Require every scheduled region to strictly reduce the latency model's
  estimated completion cycles. A legal but unprofitable first version measured
  about 0.7% slower locally; the profitability gate removes arbitrary physical
  reordering while retaining schedules that separate high-latency definitions
  from their consumers.

### Validation

- RISC-V directed tests: 59/59 passed across `-O0` and `-O1`.
- Functional and hidden-functional suites: 109/109 and 40/40 passed at `-O1`.
- 2026 RISC-V functional suites: 100/100 and 40/40 passed at `-O1`.
- 2026 RISC-V performance suite: 60/60 passed at `-O1`.
- Historical RISC-V performance suites: 57/57 and 60/60 passed at `-O1`.

### Result

| Scope | Pre-RA scheduler only | Pre + post-RA | Change |
|---|---:|---:|---:|
| 2026 60-case dynamic instructions | 20,599,099,374 | 20,599,209,219 | effectively unchanged |
| Six-run wall-clock median | 8.405 s | 8.365 s | -0.48% |

Five of the six paired whole-suite timings were no slower with profitable
post-RA scheduling. The local difference remains small enough to require
platform A/B confirmation, but unlike the unconstrained experiment it is both
correct and directionally positive without changing spill behavior.

## 12. Late machine-SSA instruction combining

### Change

- Add an explicit SSA-safe mode to the RISC-V peephole pass. It retains local
  constant folding, immediate formation and algebraic identities, while
  pressure-sensitive `sext.w` coalescing, single-use chain folding and
  compare-to-branch folding remain in the post-Phi pass.
- Run `Peephole -> AddrFold -> DCE` to a bounded fixpoint after pre-RA
  scheduling and immediately before machine-SSA verification. Placing these
  combines after scheduling is important: running them inside the main
  fixpoint changes scheduling priorities and caused about 7.2% more dynamic
  instructions in `huffman` through different allocation/spill decisions.
- Make compare-to-branch folding genuinely local. The implementation previously
  claimed the comparison and branch were in one block but did not check it;
  under SSA it could chase a unique definition across blocks and extend two
  compare operands through the CFG where only one boolean had been live.
- Keep post-Phi LICM for now. Removing it still increases the 60-case total by
  roughly 267 million instructions. Moving the complete, pressure-sensitive
  peephole plus LICM before Phi recovers part of that loss but regresses the
  best SSA-safe configuration, so the remaining migration is deferred to the
  Phi-aware allocator stage.

### Validation

- RISC-V directed tests: 59/59 passed across `-O0` and `-O1`.
- Functional and hidden-functional suites: 109/109 and 40/40 passed at `-O1`.
- 2026 RISC-V functional suites: 100/100 and 40/40 passed at `-O1`.
- 2026 RISC-V performance suite: 60/60 passed at `-O1`.
- Historical RISC-V performance suites: 57/57 and 60/60 passed at `-O1`.

### Result

| Scope | M5 | M6.1 | Change |
|---|---:|---:|---:|
| 2026 60-case dynamic instructions | 20,599,209,219 | 20,544,114,152 | -0.27% |
| 2026 60-case geometric mean | 1.0000 | 0.9961 | -0.39% |
| `crc1/2/3` | baseline | baseline -4.14% | -4.14% each |
| `matmul1` | 120,242,573 | 116,268,845 | -3.31% |
| `matmul3` | 225,303,855 | 217,557,843 | -3.44% |

The raw total reduction is 55,095,067 instructions, or about 0.27%; this exact
normalization corrects an earlier exploratory estimate of 0.75%. Six alternating
whole-suite wall-clock runs produced the same median of about 8.37 s for M5 and
M6.1, so local elapsed time is unchanged despite the deterministic instruction
reduction.

## 13. Phi-aware graph coloring

### Change

- Split machine Phi lowering into two phases. `PrepareEdges` materializes
  edge-local selection code and creates dedicated Phi edges while preserving
  machine SSA; actual parallel-copy emission now happens after coloring.
  Implicit fallthrough edges are made explicit when split.
- Teach graph coloring true Phi semantics: Phi results are successor-block
  definitions, incoming values are predecessor-edge uses, and each incoming
  pair contributes a coalescing affinity. Register allocation consequently runs
  before Phi destruction instead of extending every incoming value through the
  successor.
- Make spill rewriting Phi-aware. A spilled incoming value is reloaded on its
  prepared edge. A spilled Phi result is represented by edge stores to its spill
  slot; those stores are themselves sequentialized as a parallel copy, including
  memory cycles such as two spilled loop values swapping slots.
- Lower colored Phi cycles with physical-register copies. Architectural register
  identity ignores Word/QWord views, cross-class copies use the RISC-V bit-move
  instructions, and the reserved `t3` scratch register breaks physical cycles.
- Run edge preparation before the late SSA cleanup fixpoint. This gives LICM
  dedicated preheaders without destroying Phi information. LICM changes are
  transactional and rejected when they exceed a target-derived pressure budget;
  the rematerializable budget is derived from one quarter of the smaller
  allocatable register file rather than remaining fixed at three.

### Validation

- RISC-V directed tests: 60/60 passed across `-O0` and `-O1`. The new test
  combines integer and floating Phi cycles with a 28-value loop-carried cycle;
  its optimized assembly contains 119 stack accesses, so the test exercises
  Phi spilling rather than only the no-spill path.
- Functional and hidden-functional suites: 109/109 and 40/40 passed at `-O1`.
- 2026 RISC-V functional suites: 100/100 and 40/40 passed at `-O1`.
- 2026 RISC-V performance suite: 60/60 passed at `-O1`.
- Historical RISC-V performance suites: 57/57 and 60/60 passed at `-O1`.
- No ARM runtime tests were run.

### Result

| Scope | M6.1 | Phi-aware RA | Change |
|---|---:|---:|---:|
| 2026 60-case dynamic instructions | 20,544,114,152 | 20,530,627,494 | -0.066% |
| 2026 60-case geometric mean | 1.0000 | 0.9993 | -0.07% |
| `h-1-01/02/03` | baseline | baseline -9.5% to -9.3% | faster |
| `transpose0/1/2` | baseline | baseline -5.5% / -4.0% / -12.8% | faster |
| `huffman-01/02/03` | baseline | baseline about -1.7% | faster |
| `matmul1/2/3` | baseline | baseline +18% to +23% | slower |

The deterministic aggregate instruction count is slightly better, but local
wall time is not yet a win. Six alternating no-plugin QEMU runs gave medians of
8.369 s for M6.1 and 8.668 s for Phi-aware RA (about 3.6% slower). The remaining
work is therefore performance, not correctness: matrix kernels still receive a
less favorable invariant-materialization/allocation combination even though
Phi-aware coloring removes more copies and stack traffic. This stage establishes
the SSA-correct allocator foundation; it should not be presented as a runtime
speedup until that matrix-kernel gap is closed.

## 14. Spill rematerialization

### Change

- Treat uniquely defined `LI` and `LA` values with stable operands as
  rematerializable spill recipes. When graph coloring selects one of these
  values for spilling, recreate it at each ordinary use or prepared Phi edge
  instead of allocating a stack slot, storing the value, and reloading it.
- Remove the original long-lived definition after selecting rematerialization.
  Fresh short-lived temporaries remain protected by the allocator's existing
  spill-rewrite convergence rule.
- Keep the policy deliberately local to the selected spill set. Experiments
  that globally favored rematerializable candidates were rejected because a
  coalesced Phi representative is not equivalent to a standalone SSA value.
- This follows LLVM's MachineLICM and greedy-register-allocation model: cheap
  instructions should not lengthen live ranges under pressure, while truly
  rematerializable values can be recreated close to their uses.

### Validation

- RISC-V directed tests: 60/60 passed across `-O0` and `-O1`.
- Functional and hidden-functional suites: 109/109 and 40/40 passed at `-O1`.
- 2026 RISC-V functional suites: 100/100 and 40/40 passed at `-O1`.
- 2026 RISC-V performance suite: 60/60 passed at `-O1`.
- Historical RISC-V performance suites: 57/57 and 60/60 passed at `-O1`.
- No ARM runtime tests were run.

### Result

| Scope | Phi-aware RA | + rematerialization | Change |
|---|---:|---:|---:|
| 2026 60-case dynamic instructions | 20,530,627,494 | 20,524,584,941 | -6,042,553 (-0.029%) |
| Alternating no-plugin QEMU median | 8.68 s | 8.62 s | -0.06 s (-0.69%) |

The deterministic reduction is concentrated in the `many_mat_cal` family,
where avoiding stack-backed invariant constants saves about 0.51% per case.
Three alternating wall-clock runs per version agree with the instruction-count
direction, but the 0.06 s local difference is small enough to treat as a modest
rather than architectural runtime win.
