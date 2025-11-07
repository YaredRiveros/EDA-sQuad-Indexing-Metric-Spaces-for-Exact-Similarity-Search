To build:
    g++ -O2 -std=c++17 main.cpp -o build-mvpt

Queries:

    ./build-mvpt ../data/LA.txt <size> index.mvpt <bucket_size> range <radius>

    ./build-mvpt ../data/LA.txt <size> index.mvpt <bucket_size> knn <number>

- arity is set to 5 for experimentation
