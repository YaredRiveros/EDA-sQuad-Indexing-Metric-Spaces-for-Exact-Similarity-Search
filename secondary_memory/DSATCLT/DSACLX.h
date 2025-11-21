#ifndef DSACLX_H
#define DSACLX_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "Node.h"
#include "ResultSet.h"
#include "obj.h"

extern int BlockSize;

class DSACLpage;
class DSACLfile;

void buildSecondaryMemory(char *fileName, char *dbname);
void insertSecondaryMemory(DSACLpage *page, Objvector *obj);
int  rangeSearch(Objvector query, double radius);
int  rangeSearchSecondaryMemory(DSACLpage *page, Objvector query, double radius,
                                double timeStamp, bool show);
double knnSearch(Objvector query, int k);
void knnSearchSecondaryMemory(DSACLpage *page, Objvector query, int k, int timeStamp);
void writePage(DSACLpage *page);
DSACLpage *readPage(double pageNo);
void printPage(DSACLpage *page);

class DSACLfile {
public:
    DSACLfile() : pageNum(0), fileHandle(-1), isOpen(false) {}
    void create(const char *finame, Objvector root);
    void open(const char *filename);
    void close();

    void read(double pageNo, int size, char *buf);
    void write(double pageNo, int size, const char *buf);
    DSACLpage * allocate();

    bool isOpened();
    void setOpen(bool state);

    int pageNum;

private:
    int  fileHandle;
    bool isOpen;
};

class DSACLpage {
public:
    double pageNo = 0;
    Node *node = nullptr;

    explicit DSACLpage(double pageNo) : pageNo(pageNo), node(nullptr) {}
    ~DSACLpage() { freeResources(); }

    void setNode(Node *n) { node = n; }
    int  pageSize() { return BlockSize; }
    void freeResources() { delete node; node = nullptr; }
};

#endif // DSACLX_H
