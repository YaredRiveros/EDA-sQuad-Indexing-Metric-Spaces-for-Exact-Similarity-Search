To build:
    g++ -O2 -std=c++17 main.cpp -o build-laesa

Queries:

    ./build-laesa ../data/LA.txt <size> index.laesa <num_pivots> range <radius>

    ./build-laesa ../data/LA.txt <size> index.laesa <num_pivots> knn <number>
