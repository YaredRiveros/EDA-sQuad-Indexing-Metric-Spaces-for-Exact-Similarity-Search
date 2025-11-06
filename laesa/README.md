To build:
    g++ -O2 -std=c++17 main.cpp -o build-laesa

Queries:

    ./build-laesa ../data/LA.txt <size> index.laesa <bucket_size> range <radius>

    ./build-laesa ../data/LA.txt <size> index.laesa <bucket_size> knn <number>
