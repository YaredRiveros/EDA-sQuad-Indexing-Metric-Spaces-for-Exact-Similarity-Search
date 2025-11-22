CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wno-unused-result -Wno-unused-function

TARGET = DIndex_test

all: $(TARGET)

$(TARGET): test.cpp dindex.hpp ../../objectdb.hpp ../../datasets/paths.hpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) test.cpp

clean:
	rm -f $(TARGET)
	rm -rf results/
	rm -rf dindex_indexes/

.PHONY: all clean
