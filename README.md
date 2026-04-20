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

And I use ``stages`` namespace to divide it clearly with potential other parts. But to be honest, I only use ``stages`` to do output for each stage, it is just a __Print__ function for each stage.

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
2. to be attention, I will transfer owner to ``stages`` module to do output for lexer stage, so we cannot use ``lexer`` and ``ofstream`` after that.
3. if user do not need to output lexer, we will transfer tokens to parser directly.

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

Then I'd like to introduce a few details about the ``bison`` interface design:
1. yy::parser::make_##BISONTYPE(): this kind of functions are used to generate the parser object, and this kind of __static__ functions are generated by bison to create tokens which are used in the grammar analysis.
   
   For example, I define a token called ``INTCONST``, then ``bison`` will generate a function call ``yy::parser::make_INTCONST(int value)`` to create a token of type ``INTCONST`` with the given integer value. This function will be called in the lexer when we recognize an integer constant, and it will return a token that can be used by the parser to build the AST.

    We have to use these generated functions to transfer our own tokens generated in our lexer to the tokens that bison can recognize and use in the parsing process. This way, we can ensure that our lexer and parser are properly integrated and can work together to build the AST from the source code.

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

While adding the parser rules about array definition, I had some trouble about how to handle the array indices, since the array can be defined in two different ways:
1. In function formal parameters, the array is defined with a single index, such as ``int arr[]``, which represents an array parameter. In this case, I will treat it as a pointer to the base type of the array.
2. In variable declarations and the left values, the array is defined with multiple indices, such as ``int arr[10][20]``, which represents a multi-dimensional array. In this case, I will treat it as an array type with the specified dimensions.
3. Note that the declaration of array variables' types will be decided in the semantic analysis stage instead of the parsing stage, since I want to keep the parsing stage focused on syntactic analysis and leave type checking and annotation to the semantic analysis stage. This way, we can keep the responsibilities of each stage clear and avoid mixing concerns. To achieve this, I will not do anything about the symbol table as well.

#### Printer
Since the printer will never change the AST, I will use the ``ConstASTVisitor`` to implement the printer, which ensures that the AST nodes are not modified during the printing process. This way, we can safely traverse the AST and generate a human-readable representation of the source code without worrying about unintended modifications to the AST.

### Semantic Analysis
To begin with, I will implement the symbol table to manage variable and function declarations, scopes, and types. 
#### Symbol Table Design
Since I want to manage all resources in a centralized way(The compiler context), I decided to implement the symbol table as part of the `CompilerContext`. 

Out of the design belief, I implement the ``SymbolTableContext`` class to manage the symbol tables for variables and functions separately. This class provides methods to add and look up symbols, as well as to manage scopes. The symbol table is designed to support nested scopes, which is essential for handling function definitions and blocks.

Since the variable and function are quite different, I decided to manage them separately in the symbol table, and talking about the resource management, since I didn't give any resource ownership to the symbol table, there is no need to worry about memory management in the symbol table.

#### Analyzing Process
Since the analyser needs to modify the AST to annotate nodes with type information, I will use the `ASTVisitor` interface to implement the semantic analysis visitor. This visitor will traverse the AST and perform type checking, scope management, and symbol table updates as needed.

And I would like to just introduce some import details about the semantic analysis process:
1. When visiting a variable declaration node, the ``Visit`` function will create a new symbol for the variable and add it to the symbol table. And then it will call the ``Visit`` function for each variable definition node, which will check each variable definition and add different information to the symbol table, such as the variable's name, type, and scope.

2. When visiting a function definition node, the ``Visit`` function will create a new symbol for the function and add it to the symbol table. It will alse update the ``current_function`` and then store the information about the function's parameters and return type in the symbol table.  
   
   To be idealistic, we should reset the ``current_function`` to nullptr every time we finish analyzing a function definition, but since there is no iterative function definition and we will update the ``current_function`` every time we visit a function definition node, so we can just reset it every time the semantic analysis is terminaled(whether it is terminaled successfully or with exception), which can make the code simpler and easier to maintain.

3. Since we have finished talking about definitions, I would like to talk about expressions. While visiting a expression, it is totally different from visiting a ``constant expression`` and visiting a ``non-constant expression``. For instance, when visiting a constant expression, we need to evaluate the expression and set its value and type in the AST node, which will be used for constant folding in the frontend optimization, however, when visiting a non-constant expression, we only need to check the types of the operands and set the type of the expression node without evaluating it, since it may involve variables whose values are not known at compile time.  
   
   Out of the belief of optimal efficiency, I decide to do constant folding during the semantic analysis and for constant variable including constant array type. But to be attention, __the lval of a constant array is not definitely a constant expression__, since it may be used as __a parameter in function calls__ as well as __the index maybe non-constant__.

4. Since the statements are quite different from expressions and declarations, it is much easier to analyze them, and there is not a lot of things needed to be done when visiting a statement node.

#### Initializer Handling
Since the initializer can be quite complex, and still out of the belief of optimal efficiency and elegancy, I decide to abstract the initializer handling into a separate class, which is called ``FlattenInitializer``, while the key process of handling initializer is to flatten the initializer list into a one-dimensional vector, which can make it easier to handle and assign values to the corresponding variables.

Inspired by the ISO/IEC 9899 standard, I designed the ``Object`` structure to represent the specific part of the variable which is being initialized, and the ``Address`` structure to represent the sepcific address which is being filled now, as well as designing a list of ``initializer`` class which will be introduced later. To be simple, for instance:
```c
int a[2][3] = {{1, 2, 3}, {4, 5, 6}};
/*
initially: 
    object: [3][2](which is the reverse of the dimensions)
    address: empty
after handling the total initializer list:
    object: [3], and will do 2 times of loop to fill the sub_object([3])
    address: [0], [1] corresponding to the 2 times of loop to fill the two sub_object([3])
then handling the initializer list separately:
    for the first list ({1, 2, 3}):
        object: empty, since there is no more dimension, which also means we should fill the value next time of recursion
        address: [0], [0], [0] corresponding to the 3 times of loop to fill the 3 elements in the first list
    for the second list ({4, 5, 6}):
        it is totally the same as the first list.
then entering the deepest loop:
    take the `1` for example:
        object: empty
        address: [0][0] corresponding to the address of `a[0][0]`
        then we will fill the value `1` into the address `a[0][0]`
    other values are all the same as `1`.
*/
```

Then I will introduce some details about the design of the ``Initializer`` classes:

Since I want to abstract the initializer handling to make it easier to be used while in both semantic analysis and IR generation, so I designed a parameter called ``function_pointer`` to indicate the specific behavior of the initializer handling in different stages. By this kind of design, I hope this helper class can be more reusable and flexible, and it can be used in both semantic analysis and IR generation without modifying the core logic of the initializer handling.

Since the scalar and list intializer can be extremely different, I designed a parent class called ``Initializer`` to represent the general initializer, and the symbol table will store the smart pointer of this class. Then using two inheriting classes to represent the scalar initializer and list initializer separately.

In my design, the initializer classes shall work as a intemediate representation of the values, which can make the ``initializer handling`` and the ``AST`` more decoupled,

### To Be Improved
There are some details which might be improved in the future, and I will list them below:
1. The error handling in the semantic analysis stage can handle the warnings better
2. Check the function call arguments in the semantic analysis stage, which might be handled via reorganize the type to check the function call arguments more easily.   

    For instance,
    ```c
    int f(int a[][20]) {
        return a[0][0];
    }
    int main() {
        int b[3][10] = {};
        return f(b);
    }
    ```
    
    which should report a warning in dealing with a[][20] and b[3][10], which is different. But to be simple, we will just regard them as the pointer to integer, which is the same. It can be improved in the future.

## Middleend
Since the `llvm::mlir` is pretty impressive, I suppose it will be of great functionality and efficiency if we use it as our middleend IR,
which can make the IR contain more semantic information so that the optimization is supposed to be more efficient and easier to be implemented. Moreover, it can be extremely convenient to do more optimizations in the future comparing to the traditional IR, i.e. `llvm::ir`.

The basic design of the middleend is as follows:

We will first transfer the AST to MLIR, since the MLIR is based on 
__dialect__, we will begin with designing a basic dialect which called `base`, and it will contain the basic operations and we will do some optimizations including and not limited to: polyhedral optimization, 
dialect conversion and so on. Then after that, we will transfer the `base` dialect(or some higher level dialect) to the lower intermediate representation module, which is designed to generate the ``llvmir``.
Based on the llvmir module, we will do some more optimizations. 
