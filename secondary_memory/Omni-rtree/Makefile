# Makefile for OmniR-tree benchmark

CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wno-unused-result -Wno-unused-function
TARGET = OmniRTree_test

all: $(TARGET)

$(TARGET): test.cpp omnirtree.hpp ../../objectdb.hpp ../../datasets/paths.hpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) test.cpp

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) omni_raf.bin index_omni_*
	rm -rf results

.PHONY: all run clean
