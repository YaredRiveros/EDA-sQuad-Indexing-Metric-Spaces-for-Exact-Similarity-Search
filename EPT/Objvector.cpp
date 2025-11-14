#include "Objvector.h"


Objvector::Objvector(const Objvector & obj)
{
	value = obj.value;
}

Objvector::Objvector(vector<float> v)
{
	value = v;
}

Objvector & Objvector::operator=(const Objvector & obj)
{
	if (this == &obj) {
		return *this;
	}
	else {
		value = obj.value;
	}
}

vector<float> & Objvector::getValue()
{
	return value;
}



void Objvector::setValue(vector<float> v)
{
	value = v;
}
