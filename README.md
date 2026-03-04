# A SYSY Compiler
A compiler implementation for the SYSY programming language, supporting multiple target architectures.

## Prerequisites
Make sure you have the following toolchains installed:
- RISC-V GCC: `riscv64-unknown-elf-gcc`
- ARM GCC: `aarch64-linux-gnu-gcc`

## Compile libs
Since I need to build our compiler from very scratch, I have to build our libs from sourcecode. So I provide Makefile command to build sylibs easily.

Over here, I provide different architectures libraries(RISC-V 64-bit, AArch64), and the commands are as follows:

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
In order to be more convenient, I divide our compiler into different stages, and each stage is responsible for a specific part of the compilation process. The stages are as follows:
1. Lexer Stage
2. Parser Stage
3. Semantic Analysis Stage
4. Intermediate Representation (IR) Generation Stage
5. Optimization Stage
6. Code Generation Stage

And I use ``stages`` namespace to divide it clearly with potential other parts. But to be honest, I only use ``stages`` to do output for each stage, it is just a Print function for each stage.

### Context
Before starting design ``parser`` part, I was wondering how to manage all the information and avoid unnecessary repeated codes. Initially I decided to use ``Stages`` to run each part until specific stage, but I found it would lead to a lot of repeated code. So I designed a ``Context`` class to manage all the information needed during compilation. This class holds all data structures and provides methods to access and modify them. This way, each stage can interact with the ``Context`` to get the information it needs without duplicating code. Thus the class ``Stages`` only need to do output for each stage.

### Visitor Pattern with CRTP
To handle various operations on our Abstract Syntax Tree (AST) and Intermediate Representation (IR) without using duplicated code and virtual class inheriting, I implemented the **Visitor Pattern**. This design pattern works like follow:
1. I define a base `Visitor` interface with visit methods for each type of node in the AST/IR.
2. Each node class in the AST/IR implements an `accept` method that takes a `Visitor` as an argument and calls the appropriate visit method.
3. I create concrete visitor classes that implement the `Visitor` interface to perform specific operations (e.g., type checking, code generation, optimization).
   
And to avoid the overhead of virtual function calls, I used the **Curiously Recurring Template Pattern (CRTP)**. This allows us to achieve static polymorphism, where the compiler can resolve method calls at compile time, leading to better performance. Also, it helps in reducing duplicated code. I will give an example below:

```cpp
// Base Visitor class using CRTP
template <typename Derived>
class Visitor {
public:
    void visit(NodeTypeA& node) {
        // then I will define visit for each node type
        static_cast<Derived*>(this)->visitNodeTypeA(node);
    }
};

class ConcreteVisitor : public Visitor<ConcreteVisitor> {
public:
    void visitNodeTypeA(NodeTypeA& node) {
        // Implementation for NodeTypeA
    }
    // over here, I have a function called ``visit`` in compiling.
};

// Normally I can only visit parent class, but with CRTP I can visit derived class directly from parent class and without dynamatic cast overhead.
```

Morever, in dealing with the middle layer ``exprnode`` in ast, I meet the problem that the inheriting structure will inherit from both ``ExprNode`` and ``VisitAble`` class, which will lead to a problem which is called the **Diamond Problem**. To solve this problem, I made ``VisitAble`` class to inherit from the base class directly, and use CRTP to make it work. So the final design is like this:

```cpp
template <typename Derived, typename Base = ASTNode>
class VisitAble : public Base {
 public:
  using Base::Base;
  void Accept(ASTVisitor *visitor) override { visitor->Visit(static_cast<Derived *>(this)); }
};

// the class VisitAble will inherit from Base class directly, and I can specify which base class to use.
// For example, in ExprNode, I will make a comparison. To be more convenient, I define some rules of expression:
// -> represents inheritance, such as A->B means class A inherits from class B.
/*
    Initially: class AddExprNode -> ExprNode AND VisitAble<AddExprNode>
    But : ExprNode -> ASTNode
            VisitAble<AddExprNode> -> ASTNode
    
    Now I have a diamond problem.

    Then after change:
    class AddExprNode -> VisitAble<AddExprNode, ExprNode>
    ExprNode -> ASTNode
    AddExprNode will inherit from VisitAble directly, and VisitAble will inherit from ExprNode, so there is no diamond problem.
    which means: ExprNode -> ExprNode -> ASTNode. It's a linear inheritance.
*/
```

And I want to add some more details about ``Visitor Pattern`` and how it works.
Take the ``frontend/ast/ast_expr.h`` for example, I know that the ``ASTNodePtr`` is a smart pointer that points to the base class of all AST nodes, which is ``ASTNode``. When I want to visit a specific node type, such as ``AddExprNode``, I need to downcast the ``ASTNodePtr`` to the derived class pointer type, which is ``AddExprNode*``. In C++ language, I record the specific child class and here it is ``AddExprNode``, so when I call the ``Accept`` function via ``node->visit(visitor)``, even though the ``ASTNodePtr`` is of type ``ASTNode``, the actual object is still the ``AddExprNode``. This is because of the polymorphic behavior of C++.

So when I call the ``Accept`` function even via the pointer of base class ``ASTNode``, I are still calling the sepcific function in child class and here is ``AddExprNode::Accept``. It is based on C++'s virtual function mechanism, which ensures that the correct overridden function is called based on the actual object type, not the pointer type.

#### Two separate visitors: mutable and const visitors
When designing the visitor pattern for our AST, I realized that the stages ``semantic analysis`` and ``IR generation`` are quite different in terms of whether they modify the AST or not. Semantic analysis typically needs to modify the AST to annotate nodes with type information, while IR generation usually just reads the AST without modifying it.

So I decided to create two separate visitor interfaces:
1. `ASTVisitor`: This visitor is designed for stages that need to modify the AST, such as semantic analysis. It provides non-const visit methods that allow modification of the AST nodes.
2. `ConstASTVisitor`: This visitor is intended for stages that only need to read the AST without modifying it, such as IR generation. It provides const visit methods that ensure the AST nodes are not modified during the visit.

I believe this separation helps to clarify the intent of each visitor and prevents accidental modifications to the AST during read-only operations.

## Frontend
To summarize, I offer multiple frontends, just need to obey our rules. 

Rules are following:
1.  ``lexer`` file must be ended with **.l**;
2.  ``parser`` file must be ended with **.y**;
3.  ``frontend.mk`` is used to define config which is used to decide which ``lexer`` and ``parser`` to use.

**But to be attention**: Different lexer and parser may have different kinds of input form and arguments, so there may be a little bit command needed to be changed which will be decided by which tools you are using. To be honest, now I only fit in **flex** and **bison**.

### Lexer
For ``Lexer`` Part, I designed an **abstract base class** that defines a unified interface for tokenization. This allows us to integrate different lexer implementations (such as Flex or hand-written lexers) seamlessly using the **Adapter Pattern**.

By default, I use `flex` for lexical analysis.

```bash
# Compile the lexer module
    make lexer
```

I follow ``ISO/IEC 9899`` to finish all the ``const integer and float`` recognition, and I follow the procedures listed below to do recognition and output:
1. if user need to output lexer, I will stop at lexer, and output tokens to the specified file;
2. to be attention, I will transfer owner to ``stages`` module to do output for lexer stage, so I cannot use ``lexer`` and ``ofstream`` after that.
3. if user do not need to output lexer, I will transfer tokens to parser directly.

### AST
Before starting design ``parser`` part, I need to design the AST structure to represent the syntax tree of the source code. The AST is a hierarchical representation of the program's structure, where each node corresponds to a construct in the source code (e.g., expressions, statements, declarations). But it is still different from IR tree, which is more close to machine code.

Over here, I first import ``Visitor Pattern`` to centralize different operations on AST nodes without modifying their classes. This allows us to add new operations easily by creating new visitor classes.

But I think there is something else needed to pay attention to, mainly focus on types:
1. I need to design different types for different kinds of ``decl`` and ``expr``. Until now, I haven't found any necessarity to add types for ``stmt`` nodes.
2. Ever before, I have always been struck by how to handle **array** in AST, especially when it is defined and passed as a parameter. Essentially, I decided to divide it into two different types:
   1. ``ArrayType``: used to represent array variable declaration and definition, which contains information about the base type and dimensions of the array.
   2. ``PtrType``: used to represent array parameters in function declarations, which is treated as a pointer to the base type of the array.
And when I need to alloca memory for array variables, I will calculate the total size based on the dimensions stored in ``ArrayType``. For array parameters, I will allocate memory for a pointer to the base type. But in symbol table, I still need to store the array type since it is still an array variable. I suppose this kind of design can make it easier to check types during semantic analysis.
3. Even though I used to define a lot of inherient classes for different kinds of expressions, I found that it may lead to a lot of duplicated code and make the AST structure more complex. So I change my design to combine some of the expression nodes together, such as the ``AddExprNode`` and ``MulExprNode``. In the documents, they are defined as different non-terminals, but we can use a single `BinaryExprNode` to represent all binary expressions, and use an attribute to distinguish between different operators. This way, we can reduce the number of classes in our AST and make it more manageable. However, all of these are based on the fact that we have a strong parser ``bison``, and I'm not sure whether this design can work well if we use a hand-written parser, since it may require more complex logic to handle different expression types.

### Parser
#### Parsing Design
For ``Parser`` part, I designed an **abstract base class** that defines a unified interface for parsing. This allows us to integrate different parser implementations (such as Bison or hand-written parsers) seamlessly using the **Adapter Pattern**.

By default, I still use `bison` for syntactic analysis. And since I have separated the lexer and parser, I need to make an adapter to connect them together. While traditionally lexer(Flex) and parser(Bison) are tightly coupled, I want to decouple them for better modularity and flexibility. So I designed a `FlexLexerImpl` class that acts as an adapter betIen the Flex-generated lexer and our abstract `Lexer` interface. This way, I can easily swap out the lexer implementation if needed without affecting the parser as Ill as I can also swap out the parser implementation if needed without affecting the lexer.

Based on our current design of the ``Lexer`` and ``Parser`` classes, I only require the `Lexer` instance store all the information in our ``Token`` structure, and the `Parser` will call the `GetNextToken()` method from the `Lexer` to retrieve tokens one by one during parsing. This design keeps the interaction betIen the lexer and parser simple and focused on token retrieval and I believe this kind of design can make our compiler more modular and easier to maintain. Moreover, our lexer can be either streaming lexer or buffered lexer, as long as it implements the `GetNextToken()` method to provide tokens to the parser.

Then I would like to introduce some interesting details about parser design, and maily based on ``bison`` parser:
1. Since ``bison`` parser doesn't know anything about the ``Token`` or something else, it just requires a function called ``yylex`` to get next token. So in our design, I need to implement this function to call our ``Lexer`` instance's ``GetNextToken()`` method to retrieve tokens. To achieve this, I need to override the default ``yylex`` function provided by ``bison`` and implement our own version that interacts with our `Lexer` class.
2. Aftering achieving token retrieval, I need to build AST from these tokens. In ``bison``, I can define grammar rules and associate actions with them. These actions will be executed when the corresponding grammar rule is matched during parsing. So in our design, I will implement these actions to create AST nodes and link them together based on the structure of the source code.
3. Finally, I need to provide a way to access the constructed AST after parsing is complete. I can achieve this by storing the root of the AST in ``ast_context`` inside ``compiler_context``, and providing a method to retrieve it.

And some more details about the overall management of ``AST``:
1. All AST nodes are **only** managed by ``AstContext``, which is inside ``CompilerContext``. This ensures that the memory management of AST nodes is centralized and consistent.
2. Each stage that needs to interact with the AST will receive a reference to the same ``CompilerContext`` instance. This allows all stages to access and modify the AST as needed without duplicating code or data structures.
3. Even though the ``Semantic analysis`` or ``IR generation`` stage may need to either modify or just read the AST, **The resource are still managed by ``AstContext`` inside ``CompilerContext``.** And the resource ownership will never be transferred to these stages.

#### Grammar Design
When designing the grammar for SYSY language in Bison, since there are a lot of non-terminals in the doc which are not necessary to be defined in our grammar, I decided to simplify the grammar by removing some of the unnecessary non-terminals. Because this change may cause confusion for someone who is reading both the doc and our grammar, I will give some explanations about how I simplify the grammar and combine some non-terminals together:
1. Since ``const`` is just a identifier for variable declaration, I decided to combine ``const`` and ``var`` declaration into a single non-terminal called ``decl``, which can represent both constant and variable declarations. And I can use attributes in the AST nodes to distinguish betIen constant and variable declarations. Besides, since I need to identify ``global`` and ``local`` variations, I can deal with global first, then local.
2. For function definitions, I don't have function declarations and definitions separately, and all functions are defined with their bodies as the body can be recognized as this format: ``{ stmt_list }``, so I can just define a single non-terminal called ``func_def`` to represent function definitions, but it must be followed by a ``{stmt_list}`` and I don't allow iterative function declarations. The ``stmt_list`` is a non-terminal, and it is a vector of statements, which can be empty. This way, the grammar can recognize the body of the function in following format:
   1. a list of statements
   2. a single `{}` which represents an empty body.
Note that since in SYSY grammar, it is not allowed to have functions which have no body, so I can just use ``{stmt_list}`` to represent the body of the function.

