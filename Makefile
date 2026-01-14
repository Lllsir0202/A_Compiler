# To begin with, we finish the build of syslib
.PHONY: librv libarm libs format check-tidy build clean all lexer parser

# Lexer and Parser
-include frontend.mk

# Define Src and Include
DIR := frontend middleend backend stages context utils
BUILD_DIR := objs
SRC := $(shell find $(DIR) -name '*.cpp') main.cpp
OBJS := $(SRC:%.cpp=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INCLUDE := $(shell find $(DIR) -name '*.h')
CHECK_DIR := $(DIR)
CHECK_SRC := $(shell find $(CHECK_DIR) -name '*.cpp' -not -name '$(LEXER_FILE).cpp' -not -name '$(PARSER_FILE).cpp')
CHECK_HEADER := $(shell find $(CHECK_DIR) -name '*.h' -not -name '$(LEXER_FILE).h' -not -name '$(PARSER_FILE).h')
# Compile libs
LIB_PATH := ./libs
LIB_SRC := $(LIB_PATH)/sylib.c
RISCV_GCC ?= riscv64-unknown-elf-gcc
ARM_GCC ?= aarch64-linux-gnu-gcc
RISCV_AR ?= riscv64-unknown-elf-ar
ARM_AR ?= aarch64-linux-gnu-ar

# Format and Check
CLANG_FORMAT := clang-format
CLANG_TIDY := clang-tidy

# Build Compiler
# Default to use clangd
CLANGD := clang++
GCC := gcc
CXX := $(CLANGD)

# Build configuration
INCLUDE_DIRS := -I. -Ifrontend -Imiddleend -Ibackend -Istages -Icontext -I utils
CXXFLAGS := -std=c++17 -Wall -Wextra $(INCLUDE_DIRS) -O2
TARGET := bin/compiler


all: format check-tidy build

build: $(TARGET)
	@echo "Build completed: $(TARGET)"

$(TARGET): $(OBJS)
	@mkdir -p bin
	@$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@
	@echo "$(CXX) $< to $@"

-include $(DEPS)

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
	@find . -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.hh" \) -not -name "sylib.*" -exec $(CLANG_FORMAT) -i {} +

check-tidy:
	@for file in $(CHECK_SRC) $(CHECK_HEADER); do \
		case "$$file" in \
			*.cpp) \
				clang-tidy -quiet $$file -- $(INCLUDE_DIRS) -std=c++17;; \
			*.h|*.hpp|*.hh) \
				clang-tidy -quiet $$file -- $(INCLUDE_DIRS) -std=c++17 -x c++;; \
		esac; \
		echo "Checked $$file"; \
	done

clean:
	@rm -rf bin
	@rm -rf objs
	@rm -f frontend/lexer/$(LEXER_FILE).cpp frontend/parser/$(PARSER_FILE).cpp frontend/lexer/$(LEXER_FILE).h frontend/parser/$(PARSER_FILE).h

clean-frontend:
	@rm -f frontend/lexer/$(LEXER_FILE).cpp frontend/parser/$(PARSER_FILE).cpp frontend/lexer/$(LEXER_FILE).h frontend/parser/$(PARSER_FILE).h

lexer:
	$(LEXER_TOOL) -d -o frontend/lexer/$(LEXER_FILE).cpp --header-file=frontend/lexer/$(LEXER_FILE).h frontend/lexer/$(LEXER_FILE).l

parser:
	$(PARSER_TOOL) -d -o frontend/parser/$(PARSER_FILE).cpp --defines=frontend/parser/$(PARSER_FILE).h frontend/parser/$(PARSER_FILE).y