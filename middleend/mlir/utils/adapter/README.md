# Adapter
We will provide an interface which named `Walk` to the operand, and the function will call a concrete class which inherits from `Adapter` to do the real work. This concrete class will be able to implement plenty of functionalities, such as replace a specific operand in specific operation.

Also, some optimizations might be implemented in the future as a concrete class of `Adapter`.