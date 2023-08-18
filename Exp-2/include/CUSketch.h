#ifndef _CUSKETCH_H
#define _CUSKETCH_H

#include "sketchBase.h"

template<typename T, uint32_t d, uint32_t w>
class CUSketch:public SketchBase<T> {
	private:
		uint32_t depth = d, width = calNextPrime(w);
		uint64_t h[d], s[d], n[d];

	public:
		T** matrix;

		CUSketch() {
			matrix = new T*[d];
			matrix[0] = new T[width * d];
			for (int i = 1; i < d; ++i) {
				matrix[i] = matrix[i - 1] + width;
			}

			memset(matrix[0], 0, width * d * sizeof(T));

			int index = 0;
			for (int i = 0; i < depth; ++i) {
				h[i] = GenHashSeed(index++);
				s[i] = GenHashSeed(index++);
				n[i] = GenHashSeed(index++);
			}

			this->name = std::string("CU");
		}

		uint32_t hash(Key_t key, int line) {
			return (uint32_t)(AwareHash((unsigned char *)key.str, CHARKEY_LEN, h[line], s[line], n[line]) % width);
		}

		void insert(Key_t key, uint32_t val = 1, bool scanOrNot = false) {
			int index[d] = {0};
    		T min_val = std::numeric_limits<T>::max();
			
    		for (int i = 0; i < depth; ++i) {
    			index[i] = hash(key, i);
    			min_val = std::min(min_val, matrix[i][index[i]]);
    		}

    		T update = min_val + val;
			assert(update != 0);

    		for (int i = 0; i < d; ++i)
    			matrix[i][index[i]] = std::max(matrix[i][index[i]], update);
		}
		
		uint32_t query(Key_t key) {
			T result = std::numeric_limits<T>::max();
			uint32_t pos;

			for (int i = 0; i < depth; ++i) {
				pos = hash(key, i);
				result = std::min(result, matrix[i][pos]);
			}
			return (uint32_t)result;
		}

		// void getHeavyHitters() {}

		// void getSuperspreaders() {}

		size_t getMemoryUsage() {
			return d * width * sizeof(T);
		}
};

#endif
