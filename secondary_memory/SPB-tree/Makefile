CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wno-unused-result -Wno-unused-function

TARGET = SPBTree_test

all: $(TARGET)

$(TARGET): test.cpp spbtree.hpp ../../objectdb.hpp ../../datasets/paths.hpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) test.cpp

clean:
	rm -f $(TARGET)
	rm -rf results/
	rm -rf spbtree_indexes/

.PHONY: all clean
