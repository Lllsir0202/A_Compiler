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

## Frontend
To summarize, we offer multiple frontends, just need to obey our rules. 

Rules are following:
1.  ``lexer`` file must be ended with **.l**;
2.  ``parser`` file must be ended with **.y**;
3.  ``frontend.mk`` is used to define config which is used to decide which ``lexer`` and ``parser`` to use.

**But to be attention**: Different lexer and parser may have different kinds of input form and arguments, so there may be a little bit command needed to be changed which will be decided by which tools you are using.

### Lexer
For ``Lexer`` Part, we designed an **abstract base class** that defines a unified interface for tokenization. This allows us to integrate different lexer implementations (such as Flex or hand-written lexers) seamlessly using the **Adapter Pattern**.

By default, we use `flex` for lexical analysis.

```bash
# Compile the lexer module
    make lexer
```