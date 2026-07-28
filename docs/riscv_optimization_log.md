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

## 9. Post-allocation spill and copy cleanup

### Change

- Run an RV-only cleanup immediately after graph-coloring register allocation.
- Restrict stack-slot reasoning to the private spill range returned by the
  allocator, excluding local arrays, outgoing arguments, callee-save slots, and
  arbitrary SP-relative accesses.
- Forward stores and repeated reloads while the saved physical register remains
  valid, remove stores of an unchanged reloaded value, and remove stores that
  are overwritten before any real memory read.
- Track physical-register value equivalence within a basic block and remove
  redundant integer or floating-point copies even when harmless instructions
  separate the two copies. Calls and register definitions conservatively kill
  the relevant facts.

### Validation

- Clean incremental compiler build: passed.
- RV64 compile plus GNU assemble at `-O1`: functional 112/112, hidden-functional
  40/40, performance 57/57, and final-performance 60/60.
- All 14 performance programs whose assembly changed produced identical output
  and exit status before and after the optimization under Spike.

### Result

| Scope | Baseline | Optimized | Reduction |
|---|---:|---:|---:|
| 117 performance programs, static instructions | 49,711 | 49,664 | 0.095% |
| 14 changed programs, Spike dynamic instructions | 1,258,763,234 | 1,258,087,801 | 0.054% |
| Each `03_sort2/3` and matching final sort, static instructions | 691/700 | 684/693 | 7 |
| Each `many_mat_cal` final program, static instructions | 878 | 876 | 2 |

The optimization is deliberately conservative and has no observed regression,
but most spill reloads are either in different basic blocks or lose their source
register before reuse.  The measured gain confirms that larger improvements
need rematerialization/live-range work or loop-address optimization rather than
additional local spill peepholes.

## 10. Monotone empty-loop-tail bypass

### Change

- Add an RV-only late CFG optimization after register allocation and before
  branch folding.
- Recognize a guarded counted-loop tail in which a branch enters a path that
  does nothing except increment the induction register by one, compare it with
  the same bound, and jump back.
- Redirect that branch directly to the loop exit only when every header entry is
  proven to satisfy `iv < bound`, the increment is exactly one, all skipped
  instructions are side-effect-free, and the physical IV register is dead at
  the exit.  These conditions also prove that the skipped signed increment loop
  reaches the bound without overflow.

The transform targets the `transpose*` family.  Its source continues incrementing
`j` all the way to `rowsize` after `i < j` becomes permanently true, even though
that tail has no effect other than the dead final value of `j`.

### Validation

- RV64 compile plus GNU assemble at `-O1`: functional 112/112, hidden-functional
  40/40, performance 57/57, and final-performance 60/60.
- The three `transpose*` programs and their three final-performance equivalents
  are the only programs whose assembly changes.  All six produced identical
  output and exit status before and after the optimization under Spike.
- A directed runtime-bound transpose with a 100,000-element empty tail produced
  identical output under Spike.

### Result

| Scope | Baseline | Optimized | Result |
|---|---:|---:|---:|
| Directed empty-tail case, dynamic instructions | 794,210 | 790,112 | 0.52% reduction |
| Each matching performance program, static instructions | 166 / 195 | 167 / 196 | +1 branch |

The extra static jump executes only on the useful body path; the formerly long
empty path exits immediately.  The official performance inputs follow an
existing mismatched short path under the local Spike/newlib setup, so their
local dynamic counts do not exercise the optimization and are intentionally not
reported as a speedup.  Native board timing is required for the submitted
`transpose2` result.

## 11. Pre-allocation single-use copy folding

### Change

- Fold an adjacent `tmp = op ...; dst = mv tmp` pair into `dst = op ...`
  before register allocation, for both integer and single-precision register
  classes.
- Require the temporary to have exactly one definition and one use, and require
  the destination to have exactly one definition.  The destination condition
  is essential because virtual registers created by phi lowering can have
  multiple definitions and are not SSA copy-coalescing candidates.
- Let the machine-pass fixpoint expose and fold short chains while keeping
  register class and width unchanged.

An initial broader prototype accepted multiply-defined destinations.  It
changed the output of `h-1-03` and was rejected before commit; the restricted
version preserves its output and avoids that invalid interaction with lowered
phi copies.

### Validation

- RV64 compile plus GNU assemble at `-O1`: functional 112/112,
  hidden-functional 40/40, performance 57/57, and final-performance 60/60.
- All 17 performance programs whose static instruction count changed produced
  identical filtered output and exit status before and after the optimization
  under Spike.
- An additional 13-case differential set covered matrix multiplication, FFT,
  recursive/iterative kernels, transpose, and the `h-1-03` regression; all
  outputs and exit statuses matched.

### Result

| Scope | Baseline | Optimized | Reduction |
|---|---:|---:|---:|
| 117 performance programs, static instructions | 49,856 | 49,729 | 0.255% |
| 17 statically changed programs, Spike dynamic instructions | 2,572,729,012 | 2,477,986,650 | 3.683% |
| Three final Huffman programs, Spike dynamic instructions | 1,254,577,091 | 1,160,539,436 | 7.496% |
| Three final sort programs, Spike dynamic instructions | 968,033,368 | 967,332,928 | 0.072% |

The large Huffman gain comes from shorter loop-carried live ranges changing the
allocator's coloring, not merely from deleting twelve static copies per
program.  The `sl*` programs lose one static instruction each; the `crypto*`
and many-matrix triples change by only single-digit static instructions and
showed no meaningful dynamic regression in the local runs.

## 12. Non-adjacent local copy propagation

### Change

- Extend the RV pre-allocation result-folding pass to remove a non-adjacent
  `dst = mv src` or `dst = fmv.s src` inside one basic block.
- Require `src` to have exactly one definition and one use, `dst` to have
  exactly one definition, the source definition to precede the copy in the
  same block, and every destination use to occur later in that same block.
- Replace those later destination uses with the source and remove the copy.
  Rebuild definition/use locations after every removal, so chains cannot reuse
  stale facts and phi-lowered multiply-defined registers remain excluded.

### Validation

- RV64 compile plus GNU assemble at `-O1`: functional 112/112,
  hidden-functional 40/40, performance 57/57, and final-performance 60/60.
- Only `03_sort2`, `03_sort3`, and their three final-performance equivalents
  changed. All five produced identical filtered output and exit status before
  and after the optimization under Spike.
- Three fresh generations of `03_sort2` were byte-for-byte identical.

### Result

| Scope | Baseline | Optimized | Reduction |
|---|---:|---:|---:|
| 117 performance programs, static instructions | 49,729 | 49,589 | 0.282% |
| Five changed sort programs, Spike dynamic instructions | 1,142,868,974 | 1,142,010,627 | 0.075% |
| Each changed sort program, static instructions | 663 / 672 | 635 / 644 | 28 instructions |
| `2025-8AD-57`, Spike dynamic instructions | 502,066,823 | 501,640,280 | 0.085% |
| `2025-D01-22`, Spike dynamic instructions | 378,103,588 | 377,676,737 | 0.113% |

The local copy removal shortens the allocator input enough to reduce the sort
frame from 480 to 448 bytes and eliminate several spill accesses. `03_sort3`
has a small, repeatable 319-instruction increase (0.00037%) on its different
input data even though it shares byte-identical code with `03_sort2`; the five
programs remain net positive by 858,347 dynamic instructions.

## 13. Post-allocation sign-bit extraction folding

### Change

- Fold the RV64 sequence `sraiw sign, x, 31; srliw bit, sign, 31` to the
  equivalent single instruction `srliw bit, x, 31`.
- Run the fold after register allocation. This preserves the interference graph,
  coloring, frame layout, and spill decisions; the generated assembly changes
  only by deleting a redundant shift.
- Remove the first definition only when the second instruction overwrites the
  same physical register, so the discarded sign value cannot remain live.

The sequence is especially common in signed division by two. The quotient uses
`(x + ((x >> 31) >>> 31)) >> 1` to implement truncation toward zero, but the
first arithmetic shift is unnecessary when extracting only bit 31 of an i32.
An initial instruction-selection prototype exposed the same identity before
allocation, but its shorter live ranges perturbed coloring in otherwise
unchanged programs. Moving the fold post-RA makes the result deterministic and
isolates the intended instruction saving.

### Validation

- Clean native compiler rebuild: passed.
- RV64 compile plus GNU assemble at `-O1`: functional 112/112,
  hidden-functional 40/40, performance 57/57, and final-performance 60/60
  (269/269 total).
- Differential Spike runs covered all seven changed correctness programs, all
  fourteen runnable changed performance programs, and the three final Huffman
  programs. Filtered output and exit status matched in every case.
- The former `h-1-03` register-allocation regression sentinel also matched.

### Result

| Scope | Baseline | Optimized | Reduction |
|---|---:|---:|---:|
| All 269 RV programs, static instructions | 101,908 | 100,838 | 1.050% |
| 117 performance programs, static instructions | 49,589 | 49,300 | 0.583% |
| Three final Huffman programs, Spike dynamic instructions | 1,160,446,964 | 1,065,948,069 | 8.143% |
| `2025-236-50`, Spike dynamic instructions | 949,112,282 | 871,660,651 | 8.160% |
| `2025-UNA-47`, Spike dynamic instructions | 12,517,946 | 11,610,315 | 7.251% |
| `2025-ZUM-7`, Spike dynamic instructions | 198,816,736 | 182,677,103 | 8.118% |

The large Huffman reduction comes from removing sign-bit extraction inside the
bit-processing loop rather than from a one-time prologue saving. Two large
correctness stress cases also fall from 84,534,885 to 82,828,345 dynamic
instructions (2.019%), providing independent coverage outside the performance
suite.

## 14. Post-allocation short-circuit merge bypass

### Change

- Recognize `seqz`/`snez` values that are used only by a short-circuit branch
  and a phi-edge copy, and prove that the copied value has a single equality
  branch use in the boolean merge block.
- Preserve that proof as instruction metadata through register allocation, then
  bypass the merge on the edge where the boolean value is already known. The
  post-RA rewrite removes the redundant boolean materialization and lets late
  branch folding thread the edge directly to the known final successor.
- Validate the physical-register form again before rewriting: the condition,
  result, RHS definition, forwarding chain, and two-way merge must all retain
  the expected shape. Candidates changed by spilling or other lowering are
  conservatively left untouched.

A pre-RA CFG-rewrite prototype reduced the static instruction count, but changed
the interference graph and regressed `2025-236-50` by 0.752%. Splitting the pass
into pre-RA semantic marking and post-RA rewriting preserves the existing
coloring, spill decisions, and frame layout while still shortening the executed
path.

For example, the hot Huffman loop previously executed a first comparison,
materialized its `0/1` result, and then tested that result again after the RHS
merge. On the short-circuit edge the new code branches directly to the final
successor, saving both the materialization and the merge test.

### Validation

- Clean native compiler rebuild and targeted clang-tidy analysis completed; the
  reported diagnostics are pre-existing checks in the touched pass files.
- RV64 compile plus GNU assemble at `-O1`: functional 112/112,
  hidden-functional 40/40, performance 57/57, and final-performance 60/60
  (269/269 total).
- All 33 changed programs were linked and run as before/after Spike pairs. All
  performance outputs and exit statuses matched. Fourteen changed correctness
  programs matched byte-for-byte; the two remaining programs already fault in
  the baseline and retained the same exit status and illegal address, with only
  the diagnostic PC/RA shifted by the removed instruction.
- The 17 changed performance programs were measured with contemporaneously
  linked binaries to keep the system/runtime contribution comparable.

### Result

| Scope | Baseline | Optimized | Reduction |
|---|---:|---:|---:|
| All 269 RV programs, static instructions | 100,838 | 100,474 | 0.361% |
| 117 performance programs, static instructions | 49,300 | 49,288 | 0.024% |
| 17 changed performance programs, Spike dynamic instructions | 2,261,217,374 | 2,203,011,648 | 2.574% |
| Three final Huffman programs, Spike dynamic instructions | 1,065,947,420 | 1,010,125,635 | 5.237% |
| `2025-236-50`, Spike dynamic instructions | 871,660,434 | 826,006,517 | 5.238% |
| `2025-BTH-42`, Spike dynamic instructions | 32,420,226 | 31,466,462 | 2.942% |
| `2025-MB8-51`, Spike dynamic instructions | 18,099,134 | 17,581,317 | 2.861% |
| `82_long_func`, Spike dynamic instructions | 1,743,594 | 1,653,435 | 5.171% |

The modest static reduction understates the effect: the deleted sequences sit
inside the bit-processing loops, so three Huffman inputs save 55,821,785 dynamic
instructions in aggregate. Unlike the rejected early rewrite, the final result
does not depend on a favorable register-allocation change.

## 15. RISC-V matrix register-block pressure tuning

### Change

- Keep the four-column dot-product register block, which supplies four independent
  accumulators and row-contiguous `A[k][j..j+3]` accesses.
- Use a four-way reduction unroll inside RISC-V register blocks instead of the
  general eight-way loop unroll. Ordinary loops and the AArch64 schedule remain
  at eight-way unrolling.
- Track the reduction loops created by unroll-and-jam so the smaller factor is
  applied only to those loops. This avoids weakening unrelated loop optimization.

Widening the column block to five, six, or eight columns was rejected: the extra
accumulators and addresses exceeded the available RV64 registers and introduced
hot-loop spills. Reduction factors two and three were also measured and lost to
factor four because their extra loop control outweighed the lower pressure.

### Validation

- Native compiler rebuild and RV64 GNU assembly passed.
- RV64 compile plus GNU assemble at `-O1`: functional 112/112,
  hidden-functional 40/40, performance 57/57, and final-performance 60/60
  (269/269 total).
- The runtime-bound dot-product regression (`n=5`) exercised a four-column main
  block and scalar remainder under Spike and produced the expected `19234415`.
- A deterministic 128x128 in-place matrix product produced the same checksum
  (`53417127`) before and after the change.

### Result

| Scope | Baseline | Optimized | Reduction |
|---|---:|---:|---:|
| 128x128 matrix product, Spike dynamic instructions | 12,043,346 | 11,876,346 | 1.387% |
| Same matrix product, static instructions | 752 | 658 | 12.500% |
| Each final `many_mat_cal` program, static instructions | 893 | 814 | 8.847% |

This change reduces instruction-cache footprint and loop overhead without
changing the four-column memory-access pattern that supplies the larger cache
locality benefit.

## 16. Closed form for wrapped counters

### Motivation

Three of the sixty final-performance programs seed a matrix through the same
pseudo-random generator:

```c
int get_random() {
    int n = state % 2048;
    int i = 0;
    while (i < n) { state = state + 128; state = state % 65535; i = i + 1; }
    state = state % 65535;
    return state;
}
```

`init_matrix` calls this once per element, so the inner loop runs on the order of
`n` times per call and dominates the whole program: for `2025-OKA-1` it accounts
for 1.51 billion of 1.52 billion dynamic instructions. The loop is
side-effect-free after scalar promotion turns `state` into an SSA phi, and its
trip count is available, so `DeadLoopEliminatePass` should already have deleted
it. It did not, because `base.mod` had no scalar-evolution rule at all: the
header phi never resolved to a recurrence and the value stayed opaque.

### Why the obvious closed form is wrong

`(start + step * n) % modulus` is not a valid rewrite here, for two independent
reasons. SysY `%` truncates toward zero, so it is not a ring homomorphism on
negative values -- and `state` is a global whose initial value comes from input.
A brute-force sweep over all of int32 found 215,448 mismatching starts out of
430,660 tested. Separately, `start + step * n` overflows i32 for large `n`.

What is valid is a reduction of the *step count*, resting on two facts about the
orbit:

- After one step the state lies in `(-modulus, modulus)`. While it is negative,
  adding `step < modulus` cannot trigger a reduction, so the state climbs
  additively; after at most `K = ceil(modulus / step)` steps it is non-negative.
- Once non-negative the orbit is periodic with `P = modulus / gcd(step, modulus)`.

So the step count collapses to at most `K` additive steps followed by a count
taken mod `P`, and one final `% modulus` finishes. Every intermediate value is
bounded by compile-time constants, which the pass checks against i32 before
committing rather than assuming.

Two shape details matter in practice. The MLIR loop unroller chains eight
reductions per iteration, so the recurrence carries a `fold` factor; and the
value the transform actually needs is the one on the back edge, one step further
along than the header phi, so it carries an `offset`. Both are folded into the
same step count.

### Implementation

`SCEVModAddRecExpr` is a new SCEV kind, deliberately distinct from
`SCEVAddRecExpr` so no consumer can apply affine reasoning to a sequence that
wraps. `SCEVModExpr` gives `base.mod` a representation, and absorbs
`(counter + step) % modulus` into an offset bump so a mid-chain value is
described in the same terms as the header phi. `MatchModAddRec` peels the chain
off a back-edge value, requiring every layer to share the same constants and
each step to satisfy `0 < step < modulus` -- a larger step could reduce twice in
one application, which the bounded-orbit argument excludes.
`DeadLoopEliminatePass::ExpandModAddRec` materializes the two phases branchlessly
in the preheader.

### Validation

- The emitted integer sequence was validated op-for-op against a reference
  interpreter over 55,820,106 (start, iteration) pairs spanning 1,008
  configurations of (step, modulus, fold, offset), including negative and
  past-modulus starts: zero mismatches.
- End-to-end under Spike, 20 seed values covering negatives, zero, the modulus
  and beyond, at `-O0`, `-O1` and `-O2`: all matched a native reference build.
- RV64 compile plus GNU assemble: functional 112/112 at `-O0` and `-O1`,
  hidden-functional 40/40, performance 57/57, final-performance 60/60.
  Executed-and-diffed cases pass except `30_many_dimensions`, which fails
  identically on the parent commit (verified by stashing) and is unrelated.
- Compiling the same input three times produced byte-identical assembly.

### Result

Dynamic instruction counts under Spike, inputs inlined from each `.in` file
(Spike's proxy kernel does not deliver stdin), against `riscv64-unknown-elf-gcc
-O2` on the same source:

| Program | Before | After | Reduction | gcc -O2 |
|---|---:|---:|---:|---:|
| 2025-1NE-37 | 158,737,190 | 48,024,769 | 69.7% | 184,577,155 |
| 2025-MGB-26 | 428,923,609 | 91,422,778 | 78.7% | 509,130,540 |
| 2025-OKA-1 | 1,506,191,371 | 269,743,762 | 82.1% | 1,800,519,995 |

All three now run 3.8x to 6.7x fewer instructions than `gcc -O2`, which cannot
perform this rewrite. Outputs are unchanged and match the reference build
exactly.

## 17. Tail recursion through a fall-through `if`

### Motivation

`2025-18P-6` walks a Collatz-like chain for every start value up to 500,000:

```c
int fun(int n, int dep){
	if(n==1) return dep;
	else{
		if(n%2==0) return fun(n/2,dep+1);
		else{
			if(n*3+1<=lim) return fun(n*3+1,dep+1);
			if(n*4+1<=lim) return fun(n*4+1,dep+1);
			return 7;
		}
	}
}
```

Every recursive call is in tail position, so the function should become a loop.
`gcc -O2` does exactly that and spends zero instructions in `fun`. We were
spending 422M of 429M there -- 2.90x gcc's total.

The blocker was a shape check, not the rewrite. `IsTerminalTailBlock` required
every leaf of the `scf.if` nest to end in a return or a single-operand yield. But
`if (n*3+1<=lim) return fun(...);` has *no else*, so its else region holds a bare
operand-less `scf.yield` and control falls through to the next statement. The
walker recursed into that empty region, saw a zero-operand yield, and rejected
the whole function. That shape -- a guarded tail call followed by more code -- is
the natural way to write this, so the pass was missing the common case.

### Implementation

Both walkers now take a start index, and a fall-through region is recognized
explicitly (`IsFallThroughRegion`: single block, single operand-less yield).
When one branch of a void `scf.if` falls through, its continuation is the rest of
the *enclosing* block, so `IsTerminalTailBlock` scans from `index + 1` and
`BuildTailState` builds that branch's state from the same suffix. An `if` with
both regions empty decides nothing and is skipped. A value-producing `scf.if` can
never fall through, so that combination is still refused.

### Validation

- RV64 compile plus GNU assemble at `-O0` and `-O1`: functional 112/112,
  hidden-functional 40/40, performance 57/57, final-performance 60/60. The only
  executed-and-diffed failure is `30_many_dimensions`, which fails identically on
  the parent commit and is unrelated.
- Five recursion-bearing final-performance cases were executed under Spike with
  their real inputs and all produced the expected output, each at or below gcc's
  instruction count (`2025-0XT-12` 0.95x, `2025-6U4-15` 0.98x, `2025-CXI-10`
  1.01x, `2025-3F1-17` 1.00x, `2025-FH0-62` 0.89x).
- Compiling the same input three times produced byte-identical assembly.

### Result

| Scope | Baseline | Optimized | Reduction |
|---|---:|---:|---:|
| `2025-18P-6`, Spike dynamic instructions | 429,725,842 | 335,696,917 | 21.88% |
| `2025-18P-6`, instructions attributed to `fun` | 422,350,593 | 0 | 100% |

The recursion is now entirely a loop, as in gcc's output. The residual 2.27x gap
against `gcc -O2` is no longer call overhead but the loop body: the state-machine
encoding spends about five copy/branch instructions per iteration shuffling the
`(active, result)` pair, we sign-correct `n/2` in four instructions where gcc
proved the dividend non-negative and used one `sraiw`, and we emit `mulw` for
`n*3` where gcc uses `slliw`/`addw`. Those are separate, independently
addressable gaps.

### Measurement note

This case could not be measured before. Spike's proxy kernel does not deliver
stdin, so every input-driven case previously read garbage and exited early --
making an ours-vs-gcc sweep report `ratio=1.00` almost everywhere. The numbers
above come from linking both binaries against a sylib variant whose input
functions read a compiled-in copy of the `.in` file, which needs no change to the
test source and keeps the two builds directly comparable.

## 18. Skipping a pure loop to its last iteration

### Motivation

`2025-BTH-42` computes a software CRC over an array, then throws away all but the
final result:

```c
while (n) {
    ans = crc32(crc, a, n) % 19260817;
    n = n - 1;
}
putint(ans);
```

`crc` is zero and never updated, `a` is never written, and `crc32table` is fixed
before the timed region, so each call differs only in its length argument -- and
only the last assignment to `ans` survives. `gcc -O2` executes `crc32` exactly
once (1,897 instructions in total); we executed it 500 times, spending 218.4M
against gcc's 0.7M. That was the largest remaining gap on the suite at 308.61x.

This is deadness, not idempotence: the calls genuinely return different values
(len 500 gives 173, len 1 gives 0). ADCE cannot see it because the accumulator
phi *is* used after the loop, which transitively marks the entire body live.

### The transform

When a pure loop's accumulator is written every iteration and never read back,
only the final iteration is observable, so the loop can simply be *entered at its
final induction value* -- no peeling, no cloning, no CFG surgery. One phi operand
changes.

Conditions, all of which the pass checks:

- The loop is side-effect free. `HasSideEffect` already rejects every `var.set`
  and every impure call, so no iteration can write memory a later one reads; the
  body is a function of its induction variable alone.
- The loop is top-tested with a single exit, so entering with the final value runs
  exactly one iteration.
- The exit test is `iv != 0` (what `while (n)` lowers to) with a unit step. The
  last iteration is then the single value `bound - step`. A relational test would
  not pin down a unique last value without a trip count, and a non-unit step could
  step over the bound.
- Every other header phi is dead across iterations: nothing inside the loop reads
  it. A read from outside is fine, since that sees the final value either way.

Note this needs no trip count at all, which is what makes it apply here -- the
count depends on a runtime `getarray` result and is not derivable.

### The guard that keeps it honest

If the start value lies on the far side of the bound, the original loop does not
terminate at zero; it runs until the type wraps. Starting at the final value would
silently turn that into a halting loop. The rewritten entry value is therefore
selected arithmetically:

```
start' = (start is on the terminating side) ? (bound - step) : start
```

so the pathological path stays bit-identical. Verified: with `n = -3` the
transformed program still executes 21,475,320,843 instructions -- the full
wraparound -- while `n = 500` takes 484,383.

### Validation

- The rewrite was checked against a reference loop over all 5,202 combinations of
  start and bound in `[-25, 25]` for both unit step directions, comparing the
  final body value, the exit induction value, and whether the loop terminated at
  all: zero mismatches, including the non-terminating cases.
- Negative-path behaviour confirmed under Spike as described above.
- Three hand-written cases confirm the pass declines when it must: an accumulator
  read inside the loop (`ans = f(n) + ans/2`), a store inside the loop
  (`arr[n] = ans`), and a zero-trip loop. All three report `skipped-to-last: 0`
  and produce the reference output. The counting-up direction was also exercised.
- Across the 60 final-performance cases the transform fires on exactly one loop,
  which is the intent for a rewrite this aggressive.
- RV64 compile plus GNU assemble at `-O0` and `-O1`: functional 112/112,
  hidden-functional 40/40, performance 57/57, final-performance 60/60. The only
  executed-and-diffed failure is `30_many_dimensions`, which fails identically on
  the parent commit.
- Nineteen final-performance cases were executed under Spike with their real
  inputs; every one produced the expected output and none regressed.
- Compiling the same input three times produced byte-identical assembly.

### Result

| Scope | Baseline | Optimized | Reduction |
|---|---:|---:|---:|
| `2025-BTH-42`, Spike dynamic instructions | 218,427,209 | 707,454 | 99.68% |
| Same, ratio against `gcc -O2` (707,769) | 308.61x | 1.00x | -- |

A 308x reduction, now marginally ahead of `gcc -O2`.

### Also in this change

`ComputeExitCount` had no rule for `base.int2bool`, so `while (n)` -- a truthiness
cast rather than a comparison -- yielded no trip count for an entire family of
loops. Teaching it that `int2bool x` means `x != 0` gives the three 32-iteration
bit loops in this same program derivable trip counts (previously unknown). The
symbolic-distance case is deliberately left uncomputable: see the comment in the
`NE` branch for why clamping it would be wrong.
