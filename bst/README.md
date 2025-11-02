To build:
    g++ -O2 -std=c++17 main.cpp -o build-bst

Queries:

    ./build-bst ../data/LA.txt <size> index.bst <bucket_size> range <radius>

    ./build-bst ../data/LA.txt <size> index.bst <bucket_size> knn <number>
