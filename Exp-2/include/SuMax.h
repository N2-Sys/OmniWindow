#ifndef _SUMAX_H
#define _SUMAX_H

#include "sketchBase.h"

template<typename T, uint32_t d, uint32_t w>
class SuMaxSketch:public SketchBase<T> {
	public:
		int depth = d, width = calNextPrime(w);
		uint64_t h[d], s[d], n[d];

		T** matrix;
		
		SuMaxSketch() {
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

			this->name = std::string("SM");
		}

		inline uint32_t hash(Key_t key, int line) {
			return (uint32_t)(AwareHash((unsigned char *)key.str, CHARKEY_LEN, h[line], s[line], n[line]) % width);
		}

		void insert(Key_t key, T val = 1, bool scanOrNot = false) {
			uint32_t pos;
			T omiga = std::numeric_limits<T>::max();

			for (int i = 0; i < depth; ++i) {
				pos = hash(key, i);
				T tmp = matrix[i][pos] + val;
				assert(tmp != 0);
				if (tmp < omiga) {
					matrix[i][pos] = tmp;
					omiga = tmp;
				}
				else if (matrix[i][pos] < omiga) {
					matrix[i][pos] = omiga;
				}
				else {
					continue;
				}
			}
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

		void clear() {
			memset(matrix[0], 0, width * d * sizeof(T));
		}

		size_t getMemoryUsage() {
			return depth * width * sizeof(T);
		}
};

#endif
