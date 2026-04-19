# Dialect
This directory contains the implementation of MLIR dialects. Each dialect is defined in a separate subdirectory, and the implementation of the dialect is contained in that subdirectory.

The README aims to introduce the basic rules and some format requirements for implementing a new dialect. It also provides some examples of how to implement a new dialect.

## Basic Rules and format requirements
1. Each dialect should be defined in a separate subdirectory under `middleend/mlir/dialect/`.
2. Every dialect should have a `basic.h` file to define a base class(namely `<Dialect_Name>Ops`)  that inherits from `mlir::Operation` and defines a static variable __the dialect name__ for all the operations in the dialect which will inherit from this base class.  
for instance, for the `base` dialect, we will define a `BaseOps` class in `basic.h`, and set the __DIALECT_NAME__ to "base". Then all the operations in the `base` dialect will inherit from `BaseOps`.
3. TODO(loki): more rules will be added when we finish the ``Builder`` part.