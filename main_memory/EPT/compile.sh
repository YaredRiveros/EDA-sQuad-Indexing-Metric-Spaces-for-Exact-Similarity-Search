#!/bin/bash
# Script de compilación para EPT
g++ -std=c++17 -O3 -w test.cpp main.cpp Interpreter.cpp Objvector.cpp Tuple.cpp -o test
