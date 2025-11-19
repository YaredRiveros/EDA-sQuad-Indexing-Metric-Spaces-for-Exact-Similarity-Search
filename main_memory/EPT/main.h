#ifndef EP_TABLE_MAIN_H
#define EP_TABLE_MAIN_H

#include <iostream>
#include <vector>
#include <cmath>
#include <sys/stat.h>
#include <fstream>
#include <time.h>
#include <string>
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <limits>

#define MPivots 40

class Tuple;
class Interpreter;
class Objvector;

class SortEntry
{
public:
    int id;
    double dist;
    bool operator < (const SortEntry &a) const
    {
        return (dist < a.dist);
    }
    SortEntry() : id(-1), dist(0.0) {}
    SortEntry(const SortEntry &s) : id(s.id), dist(s.dist) {}
};

#endif
