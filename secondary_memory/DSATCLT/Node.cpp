#include "Node.h"
extern int BlockSize;
int sizeOfNode() {
	return BlockSize - sizeof(double);
}
int sizeOfObjvector() {
	return sizeof(int) + dimension * sizeof(float);
}
int sizeOfObjentry() {
	return 2 * sizeof(double) + sizeOfObjvector();
}

Node::Node(Objvector center) {
	this->center = center;
	this->cNum = 0;
	this->time = CurrentTime++;
	this->radius = 0;
	this->childNum = 0;
	this->firstChild = -1;
	this->nextSibling = -1;
	//this->cluster = (ObjEntry**)malloc((ClusterSize + 1)*sizeof(ObjEntry*));
	//memset(this->cluster, 0, (ClusterSize + 1) * sizeof(ObjEntry));
	this->cluster = new ObjEntry[ClusterSize + 1]();
	//for (int i = 0; i < ClusterSize + 1; i++) {
	//	this->cluster[i].obj.value = NULL;
	//}
}

void Node::addElement(Objvector* obj,double dist) {
	int pos = 0;
	for (int i = cNum - 1; i >= 0; i--) {
		if (cluster[i].dist < dist) {
			pos = i+1;
			break;
		}
	}

	for (int i = cNum; i > pos; i--) {
		cluster[i] = cluster[i - 1];
	}

	//ObjEntry *ele = new ObjEntry(obj, dist);
	//cluster[pos] = *ele;

	cluster[pos].obj = obj;
	cluster[pos].dist = dist;
	cluster[pos].timeStamp = CurrentTime++;
	
	cNum++;
}

void Node::freeResources() {
	
	if (cluster != NULL) {				
		delete[] cluster;
		cluster = NULL;
	}	
}

Node::~Node() {
	freeResources();
}