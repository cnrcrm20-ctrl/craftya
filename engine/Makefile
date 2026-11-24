CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter
INC      := -Iengine/include /dev/null 2>/dev/null || true
INC      := -Iengine/include

.PHONY: all test clean

all: build/craftya_test

build/craftya_test: engine/tests/test_engine.cpp engine/src/mesher.cpp engine/src/physics.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) $^ -o $@

test: build/craftya_test
	@./build/craftya_test

clean:
	rm -rf build
