#pragma once
#ifndef __CACHE__H
#define __CACHE__H

#include <iostream>
#include <vector>

#include <io.h>
extern double PageFault;

extern int BLOCK_SIZE;


using namespace std;

class CacheBlock
{
public:
	unsigned long page;
	char * data;

	CacheBlock()
	{
		data = NULL;
	}
	CacheBlock(const CacheBlock& CB)
	{
		page = CB.page;
		data = CB.data;

	}
	~CacheBlock()
	{
		data = NULL;
	}

};

class Cache
{
	int size;
	int maxSize;
	string filename;

	ifstream ifile;
	vector<CacheBlock> blockVector;
public:
	
	Cache(string fn, int maxSize) { 
		size = 0; 
		this->maxSize = maxSize; 
		filename = fn;
		ifile.open(filename, ios::in|ios::binary);
	}


	~Cache()
	{
		vector<CacheBlock>::iterator it;
		for (it = blockVector.begin(); it != blockVector.end(); it++)
			delete[](*it).data;
		size = 0;
		
		ifile.close();
	}

	bool isFull()
	{
		return (size == maxSize) ? true : false;
	}

	void reset()
	{
		vector<CacheBlock>::iterator it;
		for (it = blockVector.begin(); it != blockVector.end(); it++)
			delete[](*it).data;
		size = 0;
		blockVector.clear();
	}

	
	CacheBlock &getBlock(unsigned long page)
	{
		bool flag = false; int position = 0;
		vector<CacheBlock>::iterator it;
		
		for (it = blockVector.begin(), position = 0; it != blockVector.end(); it++, position++)
			if ((*it).page == page)
			{
				if (position != 0)
					advanceBlock(position);
				flag = true;
				
				return blockVector[0];
			}
		if (!flag)
		{
			PageFault++;
			CacheBlock curBlock;
			curBlock.page = page;
			curBlock.data = new char[BLOCK_SIZE];
			
			ifile.seekg(page*BLOCK_SIZE, ios::beg);
			ifile.read(curBlock.data, BLOCK_SIZE);
			
			addBlock(curBlock);
		}
		
		return blockVector[0];
	}

	void addBlock(CacheBlock newBlock) ///ok
	{
		if (size + 1>maxSize)
			lruStrategy(newBlock);
		else
		{
			blockVector.insert(blockVector.begin(), newBlock);
			size++;
		}
	}

	void lruStrategy(CacheBlock newBlock) ///ok
	{
		int position = maxSize - 1;

		delete[] blockVector[position].data;
		blockVector.erase(blockVector.begin() + position);
		blockVector.insert(blockVector.begin(), newBlock);
	}

	void advanceBlock(int position) ///ok
	{
		CacheBlock recBlock = blockVector[position];

		blockVector.erase(blockVector.begin() + position);
		blockVector.insert(blockVector.begin(), recBlock);
	}
};

#endif