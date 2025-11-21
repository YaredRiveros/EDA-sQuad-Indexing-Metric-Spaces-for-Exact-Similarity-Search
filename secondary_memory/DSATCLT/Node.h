#ifndef _NODEH_
#define _NODEH_

#include<cstdlib>
#include"obj.h"
extern int MaxArity;
extern int ClusterSize;
extern double CurrentTime;

int sizeOfNode();
int sizeOfObjvector();
int sizeOfObjentry();

struct ObjEntry {
	Objvector  *obj;
	double timeStamp;
	double dist; //obj与簇中心的距离

	ObjEntry(Objvector *obj,double dist) {
		this->obj = obj;
		this->dist = dist;
		this->timeStamp = CurrentTime++;
	}

	ObjEntry() {		
		obj = NULL;
	}
	
	~ObjEntry() {
		if(obj != NULL) {
			delete obj;
			obj = NULL;
		}
		
	}
};

class Node {
public:
	Objvector center;

	int cNum;//簇中结点数目
	int childNum;//孩子结点的数目
	
	double radius; //covering radius 
	double firstChild;
	double nextSibling;
	double time;
	
	ObjEntry *cluster;

	Node() {}
	Node(Objvector center);
	void addElement(Objvector* obj, double dist);
	void freeResources();
	~Node();
};
#endif