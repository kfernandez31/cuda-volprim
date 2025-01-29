# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -iquote include -I/opt/homebrew/include -Wall -Wextra -O2 -DGLM_ENABLE_EXPERIMENTAL

# Directories
SRC_DIR 	:= src
INCLUDE_DIR := include
BUILD_DIR 	:= build

# Source files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# Output binary
TARGET := main

# Default rule
all: $(BUILD_DIR) $(TARGET)

# Rule to build the target
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Rule to compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean rule
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Phony targets
.PHONY: all clean
