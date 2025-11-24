# To begin with, we finish the build of syslib
.PHONY: librv, libarm, libs

# Compile libs
LIB_PATH := ./libs
LIB_SRC := $(LIB_PATH)/sylib.c
RISCV_GCC ?= riscv64-unknown-elf-gcc
ARM_GCC ?= aarch64-linux-gnu-gcc
RISCV_AR ?= riscv64-unknown-elf-ar
ARM_AR ?= aarch64-linux-gnu-ar

# Flex and Bison
FLEX := flex
BISON := bison

librv:
	$(RISCV_GCC) -c $(LIB_SRC) -o $(LIB_PATH)/syslib_tmp.o -I $(LIB_PATH) -mcmodel=medany
	$(RISCV_AR) rcs $(LIB_PATH)/librv.a $(LIB_PATH)/syslib_tmp.o
	rm $(LIB_PATH)/syslib_tmp.o

libarm:
	$(ARM_GCC) -c $(LIB_SRC) -o $(LIB_PATH)/syslib_tmp.o -I $(LIB_PATH)
	$(ARM_AR) rcs $(LIB_PATH)/libarm.a $(LIB_PATH)/syslib_tmp.o
	rm $(LIB_PATH)/syslib_tmp.o

libs: librv libarm