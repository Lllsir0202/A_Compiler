ARCH=${1:-x86}
python functional.py ../../testcases/functional ../../testoutputs/functional 1 llvm $ARCH
python functional.py ../../testcases/h_functional ../../testoutputs/h_functional 1 llvm $ARCH
python functional.py ../../testcases/riscv_performance ../../testoutputs/riscv_performance 1 llvm $ARCH
python functional.py ../../testcases/riscv_finalperformance ../../testoutputs/riscv_finalperformance 1 llvm $ARCH
python functional.py ../../testcases/arm_performance ../../testoutputs/arm_performance 1 llvm $ARCH
python functional.py ../../testcases/arm_finalperformance ../../testoutputs/arm_finalperformance 1 llvm $ARCH
