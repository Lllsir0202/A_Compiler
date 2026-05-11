# CFG Analysis
This directory contains analysis passes that operate on the control flow graph (CFG) of a program. These passes are designed to analyze the structure and properties of the CFG, which can be useful for various optimization and transformation tasks in the compiler.

Since we most probably have two different sorts of intermediate representations in our compiler, which both in the form of control flow graphs, as well as we expect to separate the passes to run on sepecific IRs, so we have two different files to represent the CFG analysis passes for each IR.

However, the analysis passes in these two files are not totally different, as they share plenty of common analysis logic, we just need to adapt their build logic to make them work on different IRs.