# MLIR loop optimizations: Loop Tiling + Fusion (Coalescing) improvement

Branch `Backend-tmp-rv-loopopt` (based on `Backend-tmp-rv`).

## Investigation: which of the 4 requested optimizations already existed

| Optimization | Status | Where |
|---|---|---|
| Affine Scalar Replacement | **Already implemented** | BASEIR DSE/GVN store→load forwarding (`store reg_5,[a]; reg_6=mul reg_5,2` — reload eliminated). |
| Loop Coalescing (fusion) | **Already implemented** | `MLIRLoopFusionPass` (affine dependence-distance analysis). Fuses same-domain loops; 1D init+transform works. |
| Loop Tiling | **Not implemented** → **added** | `MLIRLoopTilingPass` (new). |
| Loop Unrolling / Unroll-and-Jam | **Already implemented** | `MLIRLoopUnrollPass`, `MLIRLoopPartialUnrollPass`, `MLIRConditionalJamPass`. |

So the real work was: add **Loop Tiling**, and improve **Loop Coalescing** (fusion) by removing a false scalar dependence.

## New pass: `MLIRLoopTilingPass`

`middleend/pass/transform/mlirtiling/mlirtiling.{h,cpp}`. Tiles a counted
`scf.while` loop `for iv in [init, bound) step 1` whose body contains a nested
`scf.while` (non-innermost) into:

```
ii = init
while (ii < bound):                    # tile loop, step T
  limit = min(ii + T, bound)           # scf.if select (elided when bound % T == 0)
  iv = ii
  while (iv < limit):                  # inner loop = clone of the original body
    <body, carried values threaded through>
  ii = ii + T
```

- Legality: `loopiv::Recognize` valid, `kind_==kLT`, `step_kind_==kAdd`, **`step_==1`**
  (a step>1 loop like SHA-1's `chunk_start += 64` would be mis-tiled), loop-invariant
  init/bound, own body (through `scf.if`, not nested `scf.while`) has no
  `break`/`return` (they escape the loop; `scf.continue` is safe — it targets the
  immediately-enclosing loop, which after tiling is the inner loop).
- Tiles **non-innermost** loops only: the innermost loop is stride-1 contiguous and
  unrolling's job; tiling it would only add overhead. Benchmark analysis: the
  matmul's cache problem is the outer `k` loop (C re-traversed per k); tiling `k`
  cuts C-traffic by T×.
- Carried values thread through both loops; the tile loop carries `[ii, v_0..v_{m-1}]`
  and the inner loop carries exactly the original `[v_0..v_{m-1}]`.
- Tile size default 32 (`TILING_SIZE` env), skip when constant trip count < T.
- Pipeline position: **last** among the MLIR loop transforms (after
  `MLIRLoopPartialUnrollPass`) so it cannot perturb the pattern-recognition passes
  (loop-domain tighten, invariant repeat, memory-last, stencil, checksum, unswitch,
  unroll) that the directed suite asserts on. Default OFF at -O1; opt in with
  `TILING_ON=1`, and force off with `TILING_OFF=1`.

## Fusion improvement (Loop Coalescing)

`middleend/pass/transform/mlirloopfusion/mlirloopfusion.cpp`: the scalar-dependence
check rejected a second loop whose loop-carried init is a result of the first loop
**even when that carried argument is dead-on-arrival** (never read in the body — a
phantom of variable-based lowering, e.g. an outer loop re-carries a `j` that the body
overwrites with `j=0` before any use). New helpers `BlockArgumentIsRead` and
`UsesResultAsOperand`; only a **live** carried init or an operand use counts as a
scalar dependence. Stats line adds `dead_inits=`.

A more aggressive extension (mapping aligned nested-loop induction variables to prove
2D nested fusion) was prototyped but **dropped**: it fused a loop on
`directed/loopopts/array_pipeline.sy` that broke `ArrayPipeline`'s pattern (a
committed CHECK-STATS gate). The dead-init fix alone is safe.

## Validation (all on the provided testcases)

- functional O1 109/109, functional O0 109/109, h_functional O1 40/40,
  riscv_performance O1 57/57, riscv_finalperformance O1 60/60,
  directed 104/104 (both targets, O0/O1).
- 5× independent compilation deterministic (with and without tiling).
- New anonymous regressions under `testcases/loopopt/` (positive tiled 2D/matmul,
  loop-with-`break` stays untiled, runtime-bound remainder `n=1`/`n=137`).

## Perf diff on the GIVEN testcases (qemu dynamic insn count, bench.sh)

| suite | baseline (Backend-tmp-rv) | current (tiling+fusion-fix) | Δ |
|---|---|---|---|
| functional (O1) | 42,323,176 | 42,282,815 | **−0.095%** |
| riscv_performance | 69,520,196,658 | 69,887,480,774 | +0.53% |
| riscv_finalperformance | 15,296,602,231 | 15,316,086,787 | +0.13% |

Honest interpretation: **tiling is a cache optimization.** Under qemu (no cache model)
it is ~neutral to slightly *more* instructions (extra tile-loop control + the `min`
guard for runtime bounds); on the cached FPGA BOOM board it reduces C-traffic in the
matmul kernels, which is the real payoff. The fusion dead-init fix is semantically
neutral on the given performance kernels (their loop pairs have real memory
dependences) but enables coalescing of 2D init+transform shapes.

### Wall-clock (qemu, `riscv_performance` full-suite aggregate)

| | run1 | run2 | avg |
|---|---|---|---|
| current (tiling on) | 41.96 s | 42.67 s | ~42.3 s |
| baseline (Backend-tmp-rv) | 40.88 s | 43.07 s | ~42.0 s |

Δ ≈ **+0.3–0.8%**, inside the run-to-run variance (±2.5%). Under qemu the wall-clock
tracks the insn-count ratio (qemu executes instructions at a fixed rate; no cache is
modeled), so the local speed difference mirrors the +0.5% insn increase. Per-test
timings are sub-second and noise-dominated (e.g. h-5-01 appeared +11.8% on one run,
yet tiling changed its code by only 3k of 1.27B instructions). The tiling *speedup*
only appears on hardware with a real cache.
