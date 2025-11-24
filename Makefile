# To begin with, we finish the build of syslib
.PHONY: librv, libarm, libs, format, check-tidy, build, clean

# Define Src and Include
DIR := frontend middleend backend
SRC := $(shell find $(DIR) -name '*.cpp') main.cpp
INCLUDE := $(shell find $(DIR) -name '*.h')
CHECK_DIR := $(DIR)
CHECK_SRC := $(shell find $(CHECK_DIR) -name '*.cpp')
CHECK_HEADER := $(shell find $(CHECK_DIR) -name '*.h')

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

# Format and Check
CLANG_FORMAT := clang-format
CLANG_TIDY := clang-tidy

# Build Compiler
# Default to use clangd
CLANGD := clang++
GCC := gcc
CXX ?= $(CLANGD)

# Build configuration
INCLUDE_DIRS := -I. -Ifrontend -Imiddleend -Ibackend
CXXFLAGS := -std=c++17 -Wall -Wextra $(INCLUDE_DIRS)
TARGET := bin/compiler


librv:
	$(RISCV_GCC) -c $(LIB_SRC) -o $(LIB_PATH)/syslib_tmp.o -I $(LIB_PATH) -mcmodel=medany
	$(RISCV_AR) rcs $(LIB_PATH)/librv.a $(LIB_PATH)/syslib_tmp.o
	rm $(LIB_PATH)/syslib_tmp.o

libarm:
	$(ARM_GCC) -c $(LIB_SRC) -o $(LIB_PATH)/syslib_tmp.o -I $(LIB_PATH)
	$(ARM_AR) rcs $(LIB_PATH)/libarm.a $(LIB_PATH)/syslib_tmp.o
	rm $(LIB_PATH)/syslib_tmp.o

libs: librv libarm

format:
	@find . -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.hh" \) -exec $(CLANG_FORMAT) -i {} +


check-tidy:
	@for file in $(CHECK_SRC) $(CHECK_HEADER); do \
		case "$$file" in \
			*.cpp) \
				clang-tidy -quiet $$file -- -I$(DIR) -std=c++17;; \
			*.h|*.hpp|*.hh) \
				clang-tidy -quiet $$file -- -I$(DIR) -std=c++17 -x c++;; \
		esac; \
		echo "Checked $$file"; \
	done

build:
	@echo "Building compiler..."
	@mkdir -p bin
	@if [ -n "$(SRC)" ]; then \
		$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC); \
	else \
    	echo "No source files found!"; \
    	exit 1; \
	fi

clean:
	@rm -rf bin