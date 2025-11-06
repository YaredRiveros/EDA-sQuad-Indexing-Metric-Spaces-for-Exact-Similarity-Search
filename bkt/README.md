# Burkhard–Keller Tree (BKT)

Top-down tree.


### Build:
    
    g++ -O2 -std=c++17 main.cpp -o build-bkt


### Run Queries

    ./build-bkt ../data/LA.txt <size> index.bkt <bucket_size> <step> range <radius>

    ./build-bkt ../data/LA.txt <size> index.bkt <bucket_size> <step> knn <number>


## Parameter Tuning for the BKT


To build a well-balanced **Burkhard–Keller Tree (BKT)**, you need to tune its two main construction parameters:

- **`b`** → *bucket size*: maximum number of objects per leaf before splitting  
- **`s`** → *ring width*: distance interval around each pivot  

The number of pivots in the tree **emerges naturally** from these two parameters:  
smaller `b` or `s` → more splits → more pivots.

---

### Estimate the Average Distance of Your Dataset

```cpp
double avgDist = 0;
for (int i = 0; i < 1000; i++) {
    int a = rand() % db->size();
    int b = rand() % db->size();
    avgDist += db->distance(a, b);
}
avgDist /= 1000;
```

This gives a rough estimate of the typical pairwise distance in your dataset.

---

### Choose Initial Parameters

```cpp
int bucketSize = 20;          // reasonable number of objects per leaf
double step = 0.5 * avgDist;  // moderate ring width (half of the average distance)
```

These are good starting values for most metric datasets.

---

### Evaluate the Resulting Structure

After building the index:

```cpp
cout << "Tree height: " << height(root) << endl;
cout << "Total pivots: " << countPivots(root) << endl;
```

---

### Adjust the Parameters

| Observation | Action |
|--------------|---------|
| The tree is too deep (too many pivots) | Increase `bucketSize` or `step` |
| The tree is too flat (few pivots, large buckets) | Decrease `bucketSize` or `step` |