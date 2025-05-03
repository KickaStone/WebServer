# Makefile for C++ tests

# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -g -std=c++11

# Directories
SRC_DIR = .
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
TEST_DIR = $(SRC_DIR)

# Find all test source files
TEST_SRCS = $(wildcard $(TEST_DIR)/test_*.cpp)

# Target for all tests
all: $(BUILD_DIR) $(TEST_SRCS)
	@echo "Building test executables..."
	for src in $(TEST_SRCS); do \
		exe_name=$$(basename $$src .cpp); \
		$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/$$exe_name $$src; \
	done
	@echo "All tests compiled successfully."

# Create build directory if it does not exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean up build files
clean:
	@echo "Cleaning up..."
	rm -rf $(BUILD_DIR)

.PHONY: all clean

