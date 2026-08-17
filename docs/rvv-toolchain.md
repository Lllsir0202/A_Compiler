# RVV development toolchain (local)

The system `qemu-riscv64` (6.2.0) and `riscv64-linux-gnu-gcc` (11.4) cannot
execute / assemble RVV by default. Use the following for RVV work on this box.

## Correctness + instruction counting (QEMU 8.2.2, built with plugins)

Built from source at `/tmp/qemu-8.2.2` (`riscv64-linux-user`, `--enable-plugins`):

```
QEMU_RVV=/tmp/qemu-8.2.2/build/qemu-riscv64
$QEMU_RVV -cpu rv64,v=true,vlen=256 -plugin bench/insncount.so <elf>
```

Also a fully-static QEMU 8.2.2 (no plugin support) is extracted at
`/tmp/qemu82/usr/bin/qemu-riscv64-static` (correctness only, no `-plugin`).

## Assembling RVV

`riscv64-linux-gnu-gcc` assembles raw `.s` with vector register names (v0..v31)
when `-march=rv64gcv` is passed (it lacks RVV support for inline-asm constraints,
so RVV must come from the compiler's own `.s` output, not `__asm__` in C):

```
riscv64-linux-gnu-gcc -march=rv64gcv -static foo.s -o foo.elf
```

## Notes

- Default `qemu-riscv64` CPU is `rv64gc` (no V). RVV needs `-cpu rv64,v=true,vlen=…`.
- The competition board (FPGA BOOM) may or may not expose V; this toolchain is
  for local correctness/performance validation of the vectorizer.
