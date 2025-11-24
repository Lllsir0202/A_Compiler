# A SYSY Compiler
A compiler implementation for the SYSY programming language, supporting multiple target architectures.

## Prerequisites
Make sure you have the following toolchains installed:
- RISC-V GCC: `riscv64-unknown-elf-gcc`
- ARM GCC: `aarch64-linux-gnu-gcc`

## Compile libs
Since we need to build our compiler from very scratch, we have to build our libs from sourcecode. So we provide Makefile command to build sylibs easily.

Over here, we offer different architectures libraries(RISC-V 64-bit, AArch64), and the commands are as follows:

``` bash
# Used to build all libs
    make libs
# Used to build riscv lib
    make librv
# Used to build arm lib
    make libarm
```