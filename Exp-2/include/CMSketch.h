#ifndef _CMSKETCH_H
#define _CMSKETCH_H

#include "sketchBase.h"

uint8_t test[13] = {13, 192, 191, 74, 25, 8, 252, 227, 80, 0, 118, 117, 6};

template<typename T, uint32_t d, uint32_t w>
class CMSketch:public SketchBase<T> {
	public:
		uint32_t depth = d, width = calNextPrime(w);
		uint64_t h[d], s[d], n[d];
		T** matrix;
		
		CMSketch() {
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

			this->name = std::string("CM");
		}

		inline uint32_t hash(Key_t key, int line) {
			return (uint32_t)(AwareHash((unsigned char *)key.str, CHARKEY_LEN, h[line], s[line], n[line]) % width);
		}

		void insert(Key_t key, T val = 1, bool scanOrNot = false) {
			// bool te = true;
			// for (int i = 0; i < CHARKEY_LEN; ++i) {
			// 	if ((uint8_t)key.str[i] != test[i])
			// 		te = false;
			// }
			// if (te)
			//  	std::cout << "hi" << std::endl;
			uint32_t pos;
			for (int i = 0; i < depth; ++i) {
				pos = hash(key, i);
				assert (matrix[i][pos] != std::numeric_limits<T>::max());
				matrix[i][pos] += val;
			}
		}
		
		T query(Key_t key) {
			uint32_t pos;
			T result = std::numeric_limits<T>::max();
			for (int i = 0; i < depth; ++i) {
				pos = hash(key, i);
				result = std::min(result, matrix[i][pos]);
			}
			return result;
		}

		void clear() {
			memset(matrix[0], 0, width * d * sizeof(T));
		}

		size_t getMemoryUsage() {
			return d * width * sizeof(T);
		}
};

#endif
