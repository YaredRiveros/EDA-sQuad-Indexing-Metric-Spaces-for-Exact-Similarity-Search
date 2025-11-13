To build:
    g++ -O2 -std=c++17 main.cpp -o build-bst

Queries:

    ./build-bst ../data/LA.txt <size> index.bst <bucket_size> <maxHeight> range <radius>

    ./build-bst ../data/LA.txt <size> index.bst <bucket_size> <maxHeight> knn <number>


## Test

    g++ -O2 -std=c++17 test.cpp -o build-bst

    
    ./build-bst ../data/LA.txt 100 index.bst 10 3

