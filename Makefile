# Compiler and flags
CXX := clang++
LLVM_VERSION := 19
CXXFLAGS := -Wall -Wextra -std=c++23 -ggdb3 -I/usr/lib/llvm-$(LLVM_VERSION)/include
LDFLAGS := -lclang -L/usr/lib/llvm-$(LLVM_VERSION)/lib

# Directories
SRC_DIR := src
BUILD_DIR := build

# Sources and target
SRC := $(wildcard $(SRC_DIR)/*.cpp)
OUT := $(BUILD_DIR)/cppcodegen

# Default target
all: $(BUILD_DIR) $(OUT)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
