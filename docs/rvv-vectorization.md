# RISC-V "V" (RVV) auto-vectorization

Branch: `Backend-tmp-rv-rvv` (based on `Backend-tmp-rv`). Opt-in via `-rvv`;
off by default so scalar output is byte-identical to the baseline.

## Overview

The compiler recognizes vectorizable contiguous `scf.while` loops at the MLIR
level, rewrites each into a call to a `rvv.<op>.<elem>` library intrinsic, and
expands that intrinsic into an RVV strip-mine loop in the RISC-V backend.

```
AST -> MLIR (scf.while)
   -> MLIRVectorizePass  (rewrite loops -> rvv.* intrinsics)   [only with -rvv]
   -> ... -> MIR (call rvv.*)
   -> RISCVISel::SelectRVVIntrinsic (expand to vsetvli + vector body + scalar tail)
```

The intrinsic is the transport: it is declared as a library function at MLIR and
LLVM level (like `llvm.memcpy`/`llvm.memset`), flows unchanged through
MLIR -> BASEIR -> LLVM IR -> MIR as an ordinary call, and is matched by name in
`RISCVISel::SelectCall` (`IsRVVIntrinsic`).

## Files

- `middleend/pass/transform/mlirvectorize/mlirvectorize.{h,cpp}` — the vectorizer.
- `middleend/checklibrary.ck` — marks `rvv.*` as library functions.
- `middleend/llvm/lower/lower.cpp` — declares the `rvv.*` library symbols.
- `backend/targets/riscv/isel/riscvisel.{h,cpp}` — `SelectRVVIntrinsic` (RVV codegen).
- `backend/targets/riscv/inst/riscvinst.{h,cpp}` — RVV opcodes + printer forms.
- `backend/targets/riscv/printer/riscvprinter.h` — `Vector` register class -> `vN`.
- `main.cpp` — `-rvv` flag; runs the vectorizer before loop fusion/unrolling.
- `bench/rvv_check.sh` — RVV correctness harness (compile `-rvv`, assemble
  `-march=rv64gcv`, run under an RVV-capable QEMU, diff against `.out`).

## Supported patterns

The vectorizer matches a canonical counted loop `for (i = 0; i < n; ++i)` and,
inside the single-block body, a set of independent element-wise statements

    dst_k[i] = srcA_k[i] OP srcB_k[i]

where OP is an integer (`+ - * / %`) or float (`+ - * /`) binary op, and each of
`dst_k` / `srcA_k` / `srcB_k` is a unit-stride array access through `i`. Coverage:

- single-store and multi-store loops (one intrinsic per statement);
- `int` (i32) and `float` (f32) element types;
- global arrays, stack arrays (frame slots), and inlined pointer parameters
  (array-decay `getptr(getptr(a), i)` is traced back to the array symbol);
- runtime trip counts loaded from a global (hoisted out of the loop).

Not yet handled (scalar fallback is preserved): reductions (`s += a[i]`),
scalar broadcast (`a[i] OP s`), 2-D / strided access, and cross-iteration
dependencies. A loop with any of these is left scalar, so correctness is never
compromised.

## RVV codegen

Each intrinsic `rvv.<op>.<elem>(n, dst, a, b)` expands to the standard portable
strip-mine loop (VLEN-agnostic, driven by `vsetvli`):

```
loop:
  vsetvli vl, n, e32, m1, ta, ma   # vl = min(n, VLMAX)
  vle32.v v0, (a)                  # (vfadd.vv etc. for float)
  vle32.v v1, (b)
  v<op>.vv v0, v0, v1
  vse32.v v0, (dst)
  slli off, vl, 2
  add  a, a, off / add b, b, off / add dst, dst, off
  sub  n, n, vl
  bnez n, loop
  j    exit
exit: ...
```

Vector registers (v0/v1) are used directly; the live-across-loop values (the
trip count and the three base pointers) are general-purpose virtual registers
copied into fresh vregs so the loop never mutates a register shared with later
uses. The header block ends with an explicit `j exit` (not just a conditional
back-edge) so the block-layout pass sees the fallthrough edge.

## Correctness & determinism

- RVV (qemu 8.2, `-cpu rv64,v=true,vlen=256`): functional 109/109,
  h_functional 40/40, riscv_performance 57/57, riscv_finalperformance 60/60,
  2026初赛 functional 100/100, h_functional 40/40, performance 60/60 — all pass.
- Scalar (`-rvv` off): functional 109/109, directed 104/104 (both targets, O0/O1).
- 5x independent compilation is deterministic with `-rvv` on and off.
- Toggling `-rvv` changes only the vectorized loops; the scalar code is identical.
- Anonymous regression cases under `testcases/rvv/`: positive (element-wise
  multi-store), boundary (runtime trip count n=1/n=100), negative (reduction,
  left scalar).

## Performance (instruction count, qemu 8.2 + insncount.so)

On a 100 000-element multi-store loop `c[i]=a[i]+b[i]; d[i]=a[i]*b[i]`:

| VLEN | scalar (insns) | vector (insns) | change |
|------|---------------|----------------|--------|
| 128  | 1 079 343     | 1 104 341      | +2.3%  |
| 256  | 1 079 343     | 829 341        | -23.2% |
| 512  | 1 079 343     | 691 841        | -35.9% |

The gain scales with VLEN: at VLEN=128 (4 elements/vector) the strip-mine
overhead dominates; at VLEN>=256 the vector body amortizes it. The vectorization
is emitted with no trip-count guard, so a future pass may add a runtime
`n >= VLMAX` threshold to skip vectorization of short loops on small-VLEN boards.

## Toolchain

The system `qemu-riscv64` (6.2) lacks the V extension. Local validation uses a
QEMU 8.2.2 built from source with TCG-plugin support (`/tmp/qemu-8.2.2/build/
qemu-riscv64`); assembly uses `riscv64-linux-gnu-gcc -march=rv64gcv`. See
`docs/rvv-toolchain.md`.
