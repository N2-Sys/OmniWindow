#ifndef _HASHPIPE_H
#define _HASHPIPE_H

#include "sketchBase.h"

uint8_t testKey2[CHARKEY_LEN] = {75, 144, 203, 0, 139, 169, 220, 60, 80, 0, 138, 139, 6};

template<typename T>
class Cell {
public:
	Key_t k;
	T c;

	Cell() {
		k = Key_t();
		c = 0;
	}
};

template<typename T, uint32_t d, uint32_t w>
class HashPipe:public SketchBase<T> {
	public:
		uint32_t depth = d, width = calNextPrime(w), totPkts;
		uint64_t h[d], s[d], n[d];
		double threshold;

	
		Cell<T> **matrix;

		HashPipe() {
			threshold = 0;
			matrix = new Cell<T>*[d];
			matrix[0] = new Cell<T>[width * d];
			for (int i = 1; i < d; ++i) {
				matrix[i] = matrix[i - 1] + width;
			}

			memset(matrix[0], 0, width * d * sizeof(T));
			totPkts = 0;

			int index = 0;
			for (int i = 0; i < depth; ++i) {
				h[i] = GenHashSeed(index++);
				s[i] = GenHashSeed(index++);
				n[i] = GenHashSeed(index++);
			}

			this->name = std::string("HP");
		}

		HashPipe(double thld):threshold(thld) {
			matrix = new Cell<T>*[d];
			matrix[0] = new Cell<T>[width * d];
			for (int i = 1; i < d; ++i) {
				matrix[i] = matrix[i - 1] + width;
			}

			memset(matrix[0], 0, width * d * sizeof(T));
			totPkts = 0;

			int index = 0;
			for (int i = 0; i < depth; ++i) {
				h[i] = GenHashSeed(index++);
				s[i] = GenHashSeed(index++);
				n[i] = GenHashSeed(index++);
			}
			// std::cout << "hashpipe width: " << width << std::endl;

			this->name = std::string("HP");
		}

		inline uint32_t hash(Key_t key, int line) {
			return (uint32_t)(AwareHash((unsigned char *)key.str, CHARKEY_LEN, h[line], s[line], n[line]) % width);
		}

		void insert(Key_t key, T val = 1, bool scanOrNot = false) {
			uint32_t pos;
			totPkts += val;
			// Key_t testkey = Key_t((char *)testKey2);
			// if (key == testkey)
			// 	std::cout << "hah " << val << std::endl;
			for (int i = 0; i < depth; ++i) {
				pos = hash(key, i);
				if (matrix[i][pos].k.isEmpty()) {
					matrix[i][pos].k = key;
					matrix[i][pos].c = val;
					return;
				}
				else if (matrix[i][pos].k == key){
					matrix[i][pos].c += val;
					return;
				}
				else {
					if (!i || matrix[i][pos].c < val) {
						Key_t tmp_k = matrix[i][pos].k;
						matrix[i][pos].k = key;
						key = tmp_k;
						T tmp_c = matrix[i][pos].c;
						matrix[i][pos].c = val;
						val = tmp_c;
					}
				}
			}
		}

		T query(Key_t key) {
			uint32_t pos;
			T result = 0;
			for (int i = 0; i < depth; ++i) {
				pos = hash(key, i);
				if (matrix[i][pos].k == key)
					result += matrix[i][pos].c;
			}
			return result;
		}

		void clear() {
			// std::cout << "clear\n";
			for (int i = 0; i < depth; ++i) {
				for (int j = 0; j < width; ++j) {
					matrix[i][j].c = 0;
					//memset(matrix[i][j].k.str, 0, CHARKEY_LEN);
					for (int k = 0; k < CHARKEY_LEN; ++k) {
						matrix[i][j].k.str[k] = 0;
					}
				}
			}
			totPkts = 0;
		}

		void getHeavyHitters() {
			for (int i = 0; i < depth; ++i) {
				for (int j = 0; j < width; ++j) {
					if (!matrix[i][j].k.isEmpty()) {
						T estimate = query(matrix[i][j].k);
						// if (five_tuple((char*)&testKey2) == matrix[i][j].k) {
						// 	std::cout << "HP: " << estimate << " " << threshold * totPkts << std::endl;
						// }
						if (estimate > threshold
							&& this->heavyHitters.find(matrix[i][j].k) == this->heavyHitters.end())
							this->heavyHitters.emplace(matrix[i][j].k, estimate);
					}
				}
			}
		}
};

#endif
