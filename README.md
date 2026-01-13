# A SYSY Compiler
A compiler implementation for the SYSY programming language, supporting multiple target architectures.

## Prerequisites
Make sure you have the following toolchains installed:
- RISC-V GCC: `riscv64-unknown-elf-gcc`
- ARM GCC: `aarch64-linux-gnu-gcc`

## Compile libs
Since we need to build our compiler from very scratch, we have to build our libs from sourcecode. So we provide Makefile command to build sylibs easily.

Over here, we provide different architectures libraries(RISC-V 64-bit, AArch64), and the commands are as follows:

``` bash
# Used to build all libs
    make libs
# Used to build riscv lib
    make librv
# Used to build arm lib
    make libarm
```

## Some design ideas
### Stages
In order to be more convenient, we divide our compiler into different stages, and each stage is responsible for a specific part of the compilation process. The stages are as follows:
1. Lexer Stage
2. Parser Stage
3. Semantic Analysis Stage
4. Intermediate Representation (IR) Generation Stage
5. Optimization Stage
6. Code Generation Stage

And we use ``stages`` namespace to divide it clearly with potential other parts. But to be honest, we only use ``stages`` to do output for each stage, it is just a Print function for each stage.

### Context
Before starting design ``parser`` part, I was wondering how to manage all the information and avoid unnecessary repeated codes. Initially I decided to use ``Stages`` to run each part until specific stage, but I found it would lead to a lot of repeated code. So I designed a ``Context`` class to manage all the information needed during compilation. This class holds all data structures and provides methods to access and modify them. This way, each stage can interact with the ``Context`` to get the information it needs without duplicating code. Thus the class ``Stages`` only need to do output for each stage.

### Visitor Pattern with CRTP
To handle various operations on our Abstract Syntax Tree (AST) and Intermediate Representation (IR) without using duplicated code and virtual class inheriting, we implemented the **Visitor Pattern**. This design pattern works like follow:
1. We define a base `Visitor` interface with visit methods for each type of node in the AST/IR.
2. Each node class in the AST/IR implements an `accept` method that takes a `Visitor` as an argument and calls the appropriate visit method.
3. We create concrete visitor classes that implement the `Visitor` interface to perform specific operations (e.g., type checking, code generation, optimization).
   
And to avoid the overhead of virtual function calls, we used the **Curiously Recurring Template Pattern (CRTP)**. This allows us to achieve static polymorphism, where the compiler can resolve method calls at compile time, leading to better performance. Also, it helps in reducing duplicated code. I will give an example below:

```cpp
// Base Visitor class using CRTP
template <typename Derived>
class Visitor {
public:
    void visit(NodeTypeA& node) {
        // then we will define visit for each node type
        static_cast<Derived*>(this)->visitNodeTypeA(node);
    }
};

class ConcreteVisitor : public Visitor<ConcreteVisitor> {
public:
    void visitNodeTypeA(NodeTypeA& node) {
        // Implementation for NodeTypeA
    }
    // over here, we have a function called ``visit`` in compiling.
};

// Normally we can only visit parent class, but with CRTP we can visit derived class directly from parent class and without dynamatic cast overhead.
```

## Frontend
To summarize, we offer multiple frontends, just need to obey our rules. 

Rules are following:
1.  ``lexer`` file must be ended with **.l**;
2.  ``parser`` file must be ended with **.y**;
3.  ``frontend.mk`` is used to define config which is used to decide which ``lexer`` and ``parser`` to use.

**But to be attention**: Different lexer and parser may have different kinds of input form and arguments, so there may be a little bit command needed to be changed which will be decided by which tools you are using. To be honest, now we only fit in **flex** and **bison**.

### Lexer
For ``Lexer`` Part, we designed an **abstract base class** that defines a unified interface for tokenization. This allows us to integrate different lexer implementations (such as Flex or hand-written lexers) seamlessly using the **Adapter Pattern**.

By default, we use `flex` for lexical analysis.

```bash
# Compile the lexer module
    make lexer
```

We follow ``ISO/IEC 9899`` to finish all the ``const integar and float`` recognition, and we follow the procedures listed below to do recognition and output:
1. if user need to output lexer, we will stop at lexer, and output tokens to the specified file;
2. to be attention, we will transfer owner to ``stages`` module to do output for lexer stage, so we cannot use ``lexer`` and ``ofstream`` after that.
3. if user do not need to output lexer, we will transfer tokens to parser directly.

