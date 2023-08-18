#ifndef _SPREAD_SKETCH_H
#define _SPREAD_SKETCH_H

#include "MRBP.h"
#include "sketchBase.h"

class Bucket {
public:
	MRBP v;
	IPKey_t candidate;
	uint32_t l;
	Bucket(uint32_t b, uint32_t bb, uint32_t c) {
		v = MRBP(b, bb, c);
		l = candidate = 0;
	}

	size_t getMemoryUsage() {
		return v.getMemoryUsage() + sizeof(IPKey_t) + sizeof(uint32_t);
	}
};

template<typename T, uint32_t d, uint32_t w>
class SpreadSketch:public SketchBase<T> {
public:
	std::vector<std::vector<Bucket>> matrix;
	uint32_t depth = d, width = calNextPrime(w);
	uint64_t h[d + 1], s[d + 1], n[d + 1];
	uint32_t b, bb, c;
	double threshold;

	std::set<IPKey_t> keySet;
	SpreadSketch(uint32_t _b, uint32_t _bb, uint32_t _c, double thld):b(_b), bb(_bb), c(_c), threshold(thld) {
		matrix = std::vector<std::vector<Bucket>>(d, std::vector<Bucket>(width, Bucket(_b, _bb, _c)));
		this->superspreaders.clear();
		int index = 0;
		for (int i = 0; i < d + 1; ++i) {
			h[i] = GenHashSeed(index++);
			s[i] = GenHashSeed(index++);
			n[i] = GenHashSeed(index++);
		}

		this->name = "SS";
	}

	SpreadSketch() {
		this->name = "SS";
	}

	uint32_t hash(IPKey_t key, int line) {
		return (uint32_t)(AwareHash((unsigned char *)&key, 4, h[line], s[line], n[line]) % width);
	}

	uint32_t hash2(IPKey_t key, IPKey_t dst) {
		uint64_t tmpKey = ((uint64_t)key << 32) | dst;
		return (uint32_t)(AwareHash((unsigned char *)&tmpKey, 8, h[d], s[d], n[d]) % width);
	}

	void insert_ss(IPKey_t k, IPKey_t dst, bool scanOrNot = false) {
		uint32_t x = hash2(k, dst);
		// keySet.insert(k);
		for (int i = 0; i < d; ++i) {
			uint32_t pos = hash(k, i);
			uint32_t level = matrix[i][pos].v.update(x);
			// if (i == 0 && pos == 0)
			// 	std::cout << level << " " << (uint32_t)k << std::endl;
			if (level >= matrix[i][pos].l) {
				matrix[i][pos].l = level;
				matrix[i][pos].candidate = k;
			}
		}
	}

	void getSuperspreaders() {
		for (int i = 0; i < d; ++i) {
			for (int j = 0; j < width; ++j) {
				IPKey_t key = matrix[i][j].candidate;
				keySet.insert(key);
				// std::cout << i << ' ' << j << ' ' << (uint32_t)key << std::endl;

				// int size_b = b / 8 + (b % 8 != 0);
				// int size_bb = bb / 8 + (bb % 8 != 0);

				// for (int k = 0; k < size_b; ++k) {
				// 	std::cout << i << " " << j << " " << 0 << " " << (uint32_t)matrix[i][j].v.bitmap[0][k] << std::endl;
				// }

				// for (int k = 0; k < size_b; ++k) {
				// 	std::cout << i << " " << j << " " << 1 << " " << (uint32_t)matrix[i][j].v.bitmap[1][k] << std::endl;
				// }

				// for (int k = 0; k < size_bb; ++k) {
				// 	std::cout << i << " " << j << " " << 2 << " " << (uint32_t)matrix[i][j].v.bitmap[2][k] << std::endl;
				// }
			}
		}


		// std::cout << "key in position (0,0): " << (uint32_t)matrix[0][0].candidate << std::endl;

		// std::cout << "keySet size: " << keySet.size() << std::endl;
		// std::cout << "threshold: "<< threshold << std::endl;

		for (auto i = keySet.begin(); i != keySet.end(); ++i) {
			if (*i == 0) continue;
			// std::cout << s.size() << std::endl;
			uint32_t res = std::numeric_limits<uint32_t>::max();
			for (int j = 0; j < d; ++j) {
				uint32_t pos = hash(*i, j);
				res = std::min(res, matrix[j][pos].v.query());
			}
			if (res > threshold) {
				this->superspreaders.emplace(*i, res);
				// std::cout << (uint32_t)*i << " " << res << std::endl;
			}
			// else {
			// 	std::cout << (uint32_t)*i << " " << res << std::endl;
			// }
		}
	}

	void clear() {
		keySet.clear();
		for (int i = 0; i < depth; ++i) {
			for (int j = 0; j < width; ++j) {
				matrix[i][j].l = 0;
				matrix[i][j].candidate = 0;
				matrix[i][j].v.clear();
			}
		}
	}

	void operator |= (const SpreadSketch<T, d, w> &va) {
		for (int i = 0; i < depth; ++i) {
			for (int j = 0; j < width; ++j) {
				matrix[i][j].v |= va.matrix[i][j].v;
				if (va.matrix[i][j].candidate != 0) {
					if (matrix[i][j].l <= va.matrix[i][j].l) {
						matrix[i][j].l = va.matrix[i][j].l;
						matrix[i][j].candidate = va.matrix[i][j].candidate;
						// keySet.insert(va.matrix[i][j].candidate);
					}
					// keySet.insert(va.matrix[i][j].candidate);
				}
			}
		}

		// for (auto i = va.keySet.begin(); i != va.keySet.end(); ++i) {
		// 	keySet.insert(*i);
		// }
	}

	size_t getMemoryUsage() {
		return depth * width * matrix[0][0].getMemoryUsage();
	}
};

#endif