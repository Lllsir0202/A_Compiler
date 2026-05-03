# Base Dialect
In this directory, we define the base dialect, which includes the basic and lower level representation of the MLIR. We use them as a interface to connect the high level IR and the LLVM IR. 

Since the function-about operations are defined in the `func` dialect, and there are few changes between the high level IR and the LLVM IR, currently there seems to be no need to define the function-about operations in the base dialect. However, we may add some function-about operations in the future if we find it necessary.

This directory mainly focuses on the following aspects:
1. Arithmetic operations: We seperate them into different operations according to the type of the operands, such as integer and float. 
2. Unary operations: We define some more specific unary operations, for instance, the cast operations, we seperate them into different operations according to the `from` type and the `to` type, such as `IntToFloat`, `FloatToInt`, etc.
3. Control flow operations(Which is most important): We shall define some control flow operations, such as `Branch`, `CondBranch`, etc. These operations are used to represent the control flow of the program, and they are also used to connect the high level IR and the LLVM IR.

The main usage of the `base` dialect is to lower the high level IR to the base dialect, and then lower the base dialect to the LLVM IR. I suppose this dialect will be used like: 
```
HighLevelIR -> Base Dialect -> LLVM IR
```

We will create a new `moduleOp` and lower the original `moduleOp` to the new `moduleOp` in the base dialect, and then lower the new `moduleOp` to the LLVM IR. 