#ifndef _SKETCH_BASE_H
#define _SKETCH_BASE_H

#include "utils.h"

template<typename T>
class SketchBase {
public:
	std::string name;
	std::map<Key_t, uint32_t> heavyHitters;
	std::map<IPKey_t, uint32_t> superspreaders;

	SketchBase(std::string n): name(n) {}

	SketchBase() {}

	virtual void insert(Key_t key, T val = 1, bool scanOrNot = false) {}

	virtual void insert_ss(IPKey_t k, IPKey_t dst, bool scanOrNot = false) {}

	virtual T query(Key_t key) {return 0;}

	virtual void getHeavyHitters() {}

	virtual void getSuperspreaders() {}

	virtual uint32_t getCardinality() {return 0;}

	virtual size_t getMemoryUsage() {return 0;}

	virtual void clear() {};
};

#endif