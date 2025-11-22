#!/bin/bash
# Script de compilación para GNAT
g++ -std=c++17 -O3 test.cpp db.cpp GNAT.cpp -o test
