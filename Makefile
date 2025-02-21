# Compiler and flags
CXX	:= g++
CC	:= gcc
CXXFLAGS	:= -std=c++17 -iquote include -I/opt/homebrew/include -Wall -Wextra -O3 -DGLM_ENABLE_EXPERIMENTAL
CFLAGS		:= -Iinclude -Wall -Wextra -O3  # Use CFLAGS for C files

# Directories
SRC_DIR		:= src
INCLUDE_DIR	:= include
BUILD_DIR	:= build

# Source files
CXX_SRCS := $(wildcard $(SRC_DIR)/*.cpp)
C_SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CXX_SRCS))
OBJS += $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))  # Ensure C files are compiled separately

# Output binary
TARGET := main

# Default rule
all: $(BUILD_DIR) $(TARGET)

# Rule to build the target
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Rule to compile C++ source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to compile C source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@  # Use `gcc` for C files

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean rule
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Phony targets
.PHONY: all clean
