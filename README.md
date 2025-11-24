# A SYSY Compiler

## Compile libs
Since we need to build our compiler from very scratch, we have to build our libs from sourcecode. So we give Makefile command to build sylibs easily.

Over here, we offer different architectures libs(riscv64, aarch64), and commands follows:

``` bash
# Used to build all libs
    make libs
# Used to build riscv lib
    make librv
# Used to build arm lib
    make libarm
```