# Compiler and flags
CXX      := g++
CXXSTD   := -std=c++17
WARNINGS := -Wall -Wextra -Wpedantic

BUILD_DIR := build
SRC_DIR   := src
TEST_DIR  := tests
TEST_SUPPORT := $(TEST_DIR)/support
TARGET    := p_suche
TEST_TARGET := run_tests

MAIN_SRC := $(SRC_DIR)/median_search.cpp
TEST_SRCS := $(TEST_DIR)/run_tests.cpp $(wildcard $(TEST_SUPPORT)/*.cpp)

BUILD ?= release

ifeq ($(BUILD),debug)
  CXXFLAGS := $(CXXSTD) $(WARNINGS) -g -O0 -DDEBUG
else ifeq ($(BUILD),release)
  CXXFLAGS := $(CXXSTD) $(WARNINGS) -O3 -DNDEBUG -march=native
else
  $(error Unknown BUILD=$(BUILD). Use BUILD=debug or BUILD=release)
endif

LDFLAGS :=
UNAME_S := $(shell uname -s)

TBB_PREFIX ?= $(shell brew --prefix tbb 2>/dev/null)
ifneq ($(TBB_PREFIX),)
  CXXFLAGS += -I$(TBB_PREFIX)/include
  LDFLAGS += -L$(TBB_PREFIX)/lib -ltbb
else ifneq ($(TBBROOT),)
  CXXFLAGS += -I$(TBBROOT)/include
  LDFLAGS += -L$(TBBROOT)/lib/intel64 -L$(TBBROOT)/lib64 -L$(TBBROOT)/lib -ltbb
else
  CXXFLAGS += -I/usr/include/tbb
  LDFLAGS += -ltbb -pthread
endif

ifeq ($(UNAME_S),Linux)
  GCC_MAJOR := $(shell $(CXX) -dumpversion 2>/dev/null | cut -f1 -d.)
  ifeq ($(GCC_MAJOR),8)
    LDFLAGS += -lstdc++fs
  endif
endif

.PHONY: all clean rebuild run test bench

all: $(BUILD_DIR)/$(TARGET)

test: $(BUILD_DIR)/$(TARGET) $(BUILD_DIR)/$(TEST_TARGET)
	./$(BUILD_DIR)/$(TEST_TARGET)

bench: $(BUILD_DIR)/$(TARGET) $(BUILD_DIR)/$(TEST_TARGET)
	P_SUCHE_BENCH=1 ./$(BUILD_DIR)/$(TEST_TARGET)

$(BUILD_DIR)/$(TARGET): $(MAIN_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(BUILD_DIR)/$(TEST_TARGET): $(TEST_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(TEST_SUPPORT) $(TEST_SRCS) -o $@ $(LDFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all

run: all
	./$(BUILD_DIR)/$(TARGET) --n 5 --opt size --compare --threads 0 --first --out build/median_network.txt
