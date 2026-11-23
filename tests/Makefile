# Builds and runs the game-independent rule tests. Needs only a C++17 compiler.
CXX ?= g++
SRC_DIR := ../Mods/BuildingOwnership/Source/BuildingOwnership
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O1 -g
INCLUDES := -I$(SRC_DIR)/Public
CORE_SRCS := $(wildcard $(SRC_DIR)/Private/Core/*.cpp)
BUILD := build

.PHONY: test clean
test: $(BUILD)/CoreTests
	./$(BUILD)/CoreTests

$(BUILD)/CoreTests: CoreTests.cpp $(CORE_SRCS) $(wildcard $(SRC_DIR)/Public/Core/*.h)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) CoreTests.cpp $(CORE_SRCS) -o $@

clean:
	rm -rf $(BUILD)
