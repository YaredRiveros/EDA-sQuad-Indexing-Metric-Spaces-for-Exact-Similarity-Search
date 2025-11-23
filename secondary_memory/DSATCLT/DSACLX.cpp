#include "DSACLX.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>

extern int MaxArity;
extern int ClusterSize;
extern double CurrentTime;
extern int dimension;
extern double objNum;
extern int func;

DSACLfile indexFile;
ResultSet *knnResult;

void buildSecondaryMemory(char *fileName, char *dbname) {
    using namespace std;
    using namespace std::chrono;

    ifstream db(dbname, ios::in);
    db >> dimension >> objNum >> func;

    vector<float> objVec(dimension, 0.0f);
    for (int i = 0; i < dimension; i++) db >> objVec[i];

    Objvector obj(objVec);
    indexFile.create(fileName, obj);

    // Log de parámetros de construcción
    cerr << "[build] ClusterSize: " << ClusterSize
         << " | MaxArity: " << MaxArity
         << " | BlockSize: " << BlockSize
         << " | dim: " << dimension
         << " | objs: " << static_cast<long long>(objNum)
         << "\n";

    DSACLpage *rootPage = readPage(0);

    // Progreso
    const long long N = static_cast<long long>(objNum);
    const long long STEP = 10000;      // imprime cada 10k inserciones (ajústalo si quieres)
    auto t0 = high_resolution_clock::now();

    for (long long i = 1; i < N; i++) {
        for (int j = 0; j < dimension; j++) db >> objVec[j];
        Objvector *newobj = new Objvector(objVec);
        insertSecondaryMemory(rootPage, newobj);

        if ((i % STEP) == 0 || i == N - 1) {
            auto t1 = high_resolution_clock::now();
            double secs = duration_cast<duration<double>>(t1 - t0).count();
            double pct  = (100.0 * i) / (double)N;
            double ips  = (secs > 0.0) ? (i / secs) : 0.0;
            double eta  = (ips > 0.0) ? ((N - i) / ips) : 0.0;

            cerr << "[build] inserted " << setw(10) << i << " / " << N
                 << "  (" << fixed << setprecision(1) << pct << "%)"
                 << "  speed=" << setprecision(2) << ips << " obj/s"
                 << "  ETA=" << setprecision(0) << eta << " s"
                 << "   \r" << flush;  // \r para actualizar la misma línea
        }
    }
    cerr << "\n[build] done. pages=" << indexFile.pageNum << "\n";

    delete rootPage;
    rootPage = NULL;
    db.close();
}

void insertSecondaryMemory(DSACLpage *page, Objvector *obj) {

	double dist = page->node->center.distance(*obj); 
	page->node->radius = page->node->radius > dist ? page->node->radius : dist;
	int tmpNum = page->node->cNum;
	if (tmpNum < ClusterSize || dist < page->node->cluster[tmpNum-1].dist) {
		//cout << obj << "����ҳ" << page->pageNo << endl;		
		page->node->addElement(obj, dist);
		if (page->node->cNum == ClusterSize + 1) {
			Objvector *newObj = page->node->cluster[ClusterSize].obj;
			page->node->cluster[ClusterSize].obj = NULL;
			//cout << newObj << "���Ƴ�ҳ" << page->pageNo << endl;
			page->node->cNum--;
			insertSecondaryMemory(page, newObj);
		}
		writePage(page);
	}
	else {
		double distMin = dist + 1;		
		DSACLpage **childPage = new DSACLpage*[page->node->childNum];
		int freeNum = page->node->childNum;
		int index=0;
		int lastChildPageNo = 0;
		if (page->node->childNum != 0) {			
			childPage[0] = readPage(page->node->firstChild);
			distMin = childPage[0]->node->center.distance(*obj);			
			index = 0;
			
			for (int i = 1; i < page->node->childNum;i++) {				
				childPage[i] = readPage(childPage[i - 1]->node->nextSibling);
				double tmpDist = childPage[i]->node->center.distance(*obj);
				
				if (tmpDist < distMin) { 
					distMin = tmpDist;
					index = i; 					
				}
			}
		}

		if (dist < distMin && page->node->childNum < MaxArity) {
			//cout << obj << "��Ϊ��ҳ" << page->pageNo << "�º��ӣ�" << endl;
			DSACLpage *pageNew = indexFile.allocate();
			Node *node = new Node(*obj);
			pageNew->setNode(node);
			if (page->node->childNum == 0) {
				page->node->firstChild = pageNew->pageNo;
			}
			else {
				childPage[page->node->childNum - 1]->node->nextSibling = pageNew->pageNo;
				writePage(childPage[page->node->childNum - 1]);
			}
			page->node->childNum++;
			writePage(page);
			writePage(pageNew);
			for (int i = 0; i < freeNum; i++) {						
				delete childPage[i];
				childPage[i] = NULL;
			}
		}
		else {
			for (int i = 0; i < freeNum; i++) {
				if (i != index) {				
					delete childPage[i];
					childPage[i] = NULL;
				}				
			}
			insertSecondaryMemory(childPage[index], obj);			
			writePage(page);

			delete childPage[index];
			childPage[index] = NULL;
			
		}
	
		
		delete[] childPage;
		childPage = NULL;
	}
	
}

int rangeSearch(Objvector query, double radius) {
	//cout << "��ѯ��............." << endl;
	int resultNum = 0;
	DSACLpage *rootPage = readPage(0);
	resultNum = rangeSearchSecondaryMemory(rootPage, query, radius, CurrentTime, false);
	delete rootPage;
	rootPage = NULL;
	return resultNum;
}

int rangeSearchSecondaryMemory(DSACLpage *page, Objvector query, double radius, double timeStamp, bool show) {
	int resultNum = 0;
	double dist = query.distance(page->node->center); 
	if (page->node->time < timeStamp && dist <= page->node->radius + radius) {
		if (dist <= radius) {
			resultNum++;
			if (show) {
				printobj(page->node->center);
				cout << dist << endl;
			}
		}
		int cNum = page->node->cNum;
		double rc = page->node->cluster[cNum - 1].dist;
		if (dist - radius <= rc || dist + radius <= rc) {
			for (int i = 0; i < page->node->cNum; i++) {
				if (dist - page->node->cluster[i].dist <= radius) {
					double tmpDist = query.distance(*(page->node->cluster[i].obj));// myqdistance(query, page->node.cluster[i].obj);
					if (tmpDist <= radius) {
						resultNum++;
						if (show) {
							printobj(*(page->node->cluster[i].obj));
							cout << tmpDist << endl;
						}
					}
				}
			}
			if (dist + radius < rc) return resultNum;
		}
		//�ӽ�����		
		if (page->node->childNum != 0) {			
			double *childDist = new double[page->node->childNum];
			DSACLpage **childPage = new DSACLpage*[page->node->childNum];
			
			childPage[0] = readPage(page->node->firstChild);
			childDist[0] = query.distance(childPage[0]->node->center);
			for (int i = 1; i < page->node->childNum; i++) {
				
				childPage[i] = readPage(childPage[i - 1]->node->nextSibling);
				childDist[i] = query.distance(childPage[i]->node->center);
			}
			
			double dmin = 10000;
			for (int i = 0; i < page->node->childNum; i++) {
				if (childDist[i] <= dmin + 2 * radius) {					
					double minTime = CurrentTime;
					for (int j = i + 1; j < page->node->childNum; j++) {
						if (childDist[i] > childDist[j] + 2 * radius) {
							minTime = minTime < childPage[j]->node->time ? minTime : childPage[j]->node->time;
							break;
						}
					}
					resultNum+=rangeSearchSecondaryMemory(childPage[i], query, radius, minTime,show);
				}
				dmin = dmin < childDist[i] ? dmin : childDist[i];
			}
			
			delete[] childDist;
			childDist = NULL;
			for (int i = 0; i < page->node->childNum; i++) {				
				delete childPage[i];
				childPage[i] = NULL;
			}
			
			delete[] childPage;
			childPage = NULL;
		}
	}
	return resultNum;
}

double knnSearch(Objvector query, int k) {
	knnResult = new ResultSet(k);
	DSACLpage *rootPage = readPage(0);
	knnSearchSecondaryMemory(rootPage, query, k, CurrentTime);
	delete rootPage;
	return knnResult->result[k - 1].dist;
}


void knnSearchSecondaryMemory(DSACLpage *page, Objvector query, int k, int timeStamp) {
	double dist = query.distance(page->node->center);
	if (page->node->time < timeStamp && dist <= page->node->radius + knnResult->radius) {
		if (dist <= knnResult->radius) {
			resultElem ele;
			ele.obj = page->node->center;
			ele.dist = dist;
			knnResult->addElem(ele);
		}
		int cNum = page->node->cNum;
		double rc = page->node->cluster[cNum - 1].dist;
		if (dist - knnResult->radius <= rc || dist + knnResult->radius <= rc) {
			for (int i = 0; i < page->node->cNum; i++) {
				if (dist - page->node->cluster[i].dist <= knnResult->radius) {
					double tmpDist = query.distance(*(page->node->cluster[i].obj));
					if (tmpDist <= knnResult->radius) {
						resultElem ele;
						ele.obj = *(page->node->cluster[i].obj);
						ele.dist = tmpDist;
						knnResult->addElem(ele);
					}
				}
			}
			if (dist + knnResult->radius < rc) return;
		}
		//�ӽ�����		
		if (page->node->childNum != 0) {
			
			double *childDist = new double[page->node->childNum];
			DSACLpage **childPage = new DSACLpage*[page->node->childNum];

			childPage[0] = readPage(page->node->firstChild);
			childDist[0] = query.distance(childPage[0]->node->center); 
			for (int i = 1; i < page->node->childNum; i++) {				
				childPage[i] = readPage(childPage[i - 1]->node->nextSibling);
				childDist[i] = query.distance(childPage[i]->node->center); 
			}
			
			double dmin = 10000;
			for (int i = 0; i < page->node->childNum; i++) {
				if (childDist[i] <= dmin + 2 * knnResult->radius) {					
					double minTime = CurrentTime;
					for (int j = i + 1; j < page->node->childNum; j++) {
						if (childDist[i] > childDist[j] + 2 * knnResult->radius) {
							minTime = minTime < childPage[j]->node->time ? minTime : childPage[j]->node->time;
							break;
						}
					}
					knnSearchSecondaryMemory(childPage[i], query, k, minTime);
				}
				dmin = dmin < childDist[i] ? dmin : childDist[i];
			}
			
			delete[] childDist;
			childDist = NULL;
			for (int i = 0; i < page->node->childNum; i++) {				
				delete childPage[i];
				childPage[i] = NULL;
			}
			
			delete[] childPage;
			childPage = NULL;
		}
	}
}

void writePage(DSACLpage *page) {

	char *buf = new char[page->pageSize()];
	memset(buf, 0, page->pageSize());

	int offset = 0;
	memcpy(buf + offset, &(page->pageNo), sizeof(page->pageNo));
	offset += sizeof(page->pageNo);

	memcpy(buf + offset, &(page->node->center.dim), sizeof(page->node->center.dim));
	offset += sizeof(page->node->center.dim);

	memcpy(buf + offset, page->node->center.value, page->node->center.dim * sizeof(float));
	offset += page->node->center.dim * sizeof(float);

	memcpy(buf + offset, &(page->node->cNum), sizeof(page->node->cNum));
	offset += sizeof(page->node->cNum);

	memcpy(buf + offset, &(page->node->childNum), sizeof(page->node->childNum));
	offset += sizeof(page->node->childNum);

	memcpy(buf + offset, &(page->node->radius), sizeof(page->node->radius));
	offset += sizeof(page->node->radius);

	memcpy(buf + offset, &(page->node->firstChild), sizeof(page->node->firstChild));
	offset += sizeof(page->node->firstChild);

	memcpy(buf + offset, &(page->node->nextSibling), sizeof(page->node->nextSibling));
	offset += sizeof(page->node->nextSibling);

	memcpy(buf + offset, &(page->node->time), sizeof(page->node->time));
	offset += sizeof(page->node->time);

	for (int i = 0; i < page->node->cNum; i++) {
		memcpy(buf + offset, &(page->node->cluster[i].timeStamp), sizeof(page->node->cluster[i].timeStamp));
		offset += sizeof(page->node->cluster[i].timeStamp);

		memcpy(buf + offset, &(page->node->cluster[i].dist), sizeof(page->node->cluster[i].dist));
		offset += sizeof(page->node->cluster[i].dist);

		memcpy(buf + offset, &(page->node->cluster[i].obj->dim), sizeof(page->node->cluster[i].obj->dim));
		offset += sizeof(page->node->cluster[i].obj->dim);

		memcpy(buf + offset, page->node->cluster[i].obj->value, page->node->cluster[i].obj->dim * sizeof(float));
		offset += page->node->cluster[i].obj->dim * sizeof(float);
	}

	indexFile.write(page->pageNo, page->pageSize(), buf);
	delete[] buf;	
	buf = NULL;
}

DSACLpage *readPage(double pageNo) {
	
	char *buf = new char[BlockSize];
	memset(buf, 0, BlockSize * sizeof(char));
	indexFile.read(pageNo, BlockSize, buf);

	DSACLpage *page = new DSACLpage(0);
	int offset = 0;
	memcpy(&(page->pageNo), buf + offset, sizeof(page->pageNo));
	offset += sizeof(page->pageNo);

	Objvector *objvec = new Objvector();
	memcpy(&(objvec->dim), buf + offset, sizeof(objvec->dim));
	offset += sizeof(objvec->dim);

	objvec->value = new float[objvec->dim];
	memset(objvec->value, 0, objvec->dim * sizeof(float));

	memcpy(objvec->value, buf + offset, objvec->dim * sizeof(float));
	offset += objvec->dim * sizeof(float);

	Node *node = new Node(*objvec);

	memcpy(&(node->cNum), buf + offset, sizeof(node->cNum));
	offset += sizeof(node->cNum);

	memcpy(&(node->childNum), buf + offset, sizeof(node->childNum));
	offset += sizeof(node->childNum);

	memcpy(&(node->radius), buf + offset, sizeof(node->radius));
	offset += sizeof(node->radius);

	memcpy(&(node->firstChild), buf + offset, sizeof(node->firstChild));
	offset += sizeof(node->firstChild);

	memcpy(&(node->nextSibling), buf + offset, sizeof(node->nextSibling));
	offset += sizeof(node->nextSibling);

	memcpy(&(node->time), buf + offset, sizeof(node->time));
	offset += sizeof(node->time);


	for (int i = 0; i < node->cNum; i++) {
		memcpy(&(node->cluster[i].timeStamp), buf + offset, sizeof(node->cluster[i].timeStamp));
		offset += sizeof(node->cluster[i].timeStamp);

		memcpy(&(node->cluster[i].dist), buf + offset, sizeof(node->cluster[i].dist));
		offset += sizeof(node->cluster[i].dist);

		Objvector *obj = new Objvector();
		memcpy(&(obj->dim), buf + offset, sizeof(obj->dim));
		offset += sizeof(obj->dim);

		obj->value = new float[obj->dim];

		memcpy(obj->value, buf + offset, obj->dim * sizeof(float));
		offset += obj->dim * sizeof(float);

		node->cluster[i].obj = obj;
	}

	page->setNode(node);

	delete[] buf;
	buf = NULL;
	
	delete objvec;

	return page;
}


void printPage(DSACLpage *page) {
	cout << "pageNo=" << page->pageNo << endl;
	cout << "center:{";
	printobj(page->node->center);
	cout << "}" << endl;
	cout<<"cNum=" << page->node->cNum << ",radius=" << page->node->radius << endl;
	cout << "cluster:";
	for (int i = 0; i < page->node->cNum; i++) {
		cout << "{";
		printobj(*(page->node->cluster[i].obj));
		cout << "}";
	}
	cout << endl;
	cout << "firstChild=" << page->node->firstChild << ",nextSibling=" << page->node->nextSibling << endl;
}