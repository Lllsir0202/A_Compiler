# Target Instructions
This directory contains utility functions and classes for targets. These functions and classes will be used and inherited by the target specific implementations.

This includes:
1. `TargetFunction`
2. `TargetBlock`: Set up CFG based on the TargetBlock.
3. `TargetInstruction`: This class will be used as a base class for concrete target specific instructions.
4. `TargetOperand`: This class will be used as a base class for concrete target specific operands. And it will also collect the live range and information for register allocation. In current design, the operand will include following concrete types:
   - `VirtualRegister`: This is the virtual register that will be used in the IR level. It will be allocated to physical register in the register allocation phase.
   - `PhysicalRegister`: This is the physical register that will be used in the assembly level. It will be allocated from the virtual register in the register allocation phase.
   - `Immediate`: This is the immediate value that will be used in the assembly level.
   - `FrameIndex`: This is the frame index that will be used for stack-related operations.