#pragma once
#include <string>
#include <vector>
using namespace std;

class db_t
{
public:
	virtual size_t size() const = 0;
	virtual void read(string path) = 0;
	virtual double dist(int x, int y) const = 0;
};

class double_db_t : public db_t
{
protected:
	int dimension = 0;
	vector<vector<double>> objs;
public:
	size_t size() const
	{
		return objs.size();
	}
	void read(string path);
};

class L1_db_t : public double_db_t
{
public:
	double dist(int x, int y) const;
};

class L2_db_t : public double_db_t
{
public:
	double dist(int x, int y) const;
};

class L5_db_t : public double_db_t
{
public:
	double dist(int x, int y) const;
};

class Linf_db_t : public double_db_t
{
public:
	double dist(int x, int y) const;
};

class str_db_t : public db_t
{
	vector<string> objs;
public:
	size_t size() const
	{
		return objs.size();
	}
	void read(string path);
	double dist(int x, int y) const;
};