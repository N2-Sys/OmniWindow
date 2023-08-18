#ifndef _MVSKETCH_H
#define _MVSKETCH_H

#include "sketchBase.h"

uint8_t testKey[CHARKEY_LEN] = {192, 164, 94, 251, 213, 176, 48, 93, 21, 62, 187, 1, 6};

template<typename T>
class bucket {
public:
	T v;
	Key_t k;
	T c;
	bucket() {
		v = c = 0;
	}
};

template <typename T, uint32_t d, uint32_t w>
class MVSketch:public SketchBase<T> {
	public:
		uint32_t depth = d, width = calNextPrime(w), totPkts;
		uint64_t h[d], s[d], n[d];
		bucket<T> ** matrix;
		double threshold;

		MVSketch() {
			threshold = 0;
			matrix = new bucket<T>*[d];
			matrix[0] = new bucket<T>[width * d];
			for (int i = 1; i < d; ++i) {
				matrix[i] = matrix[i - 1] + width;
			}

			totPkts = 0;

			int index = 0;
			for (int i = 0; i < depth; ++i) {
				h[i] = GenHashSeed(index++);
				s[i] = GenHashSeed(index++);
				n[i] = GenHashSeed(index++);
			}

			this->name = std::string("MV");
		}

		MVSketch(double thld):threshold(thld) {
			matrix = new bucket<T>*[d];
			matrix[0] = new bucket<T>[width * d];
			for (int i = 1; i < d; ++i) {
				matrix[i] = matrix[i - 1] + width;
			}

			totPkts = 0;

			int index = 0;
			for (int i = 0; i < depth; ++i) {
				h[i] = GenHashSeed(index++);
				s[i] = GenHashSeed(index++);
				n[i] = GenHashSeed(index++);
			}

			this->name = std::string("MV");
		}

		uint32_t hash(Key_t key, int line) {
			return (uint32_t)(AwareHash((unsigned char *)key.str, CHARKEY_LEN, h[line], s[line], n[line]) % width);
		}

		void insert(Key_t key, T val = 1, bool scanOrNot = false) {
			uint32_t pos;
			totPkts += val;
			for (int i = 0; i < depth; ++i) {
				pos = hash(key, i);
				assert (matrix[i][pos].v != std::numeric_limits<T>::max());
				matrix[i][pos].v += val;

				if (matrix[i][pos].k == key) {
					assert (matrix[i][pos].c != std::numeric_limits<T>::max());
					matrix[i][pos].c += val;
				}
				else {
					matrix[i][pos].c -= val;
					if (matrix[i][pos].c < 0) {
						matrix[i][pos].k = key;
						matrix[i][pos].c *= -1;
					}
				}

				// if (five_tuple((char*)&testKey) == matrix[i][pos].k) {
				// 	std::cout << matrix[i][pos].v << " " << matrix[i][pos].c << std::endl;
				// }
			}
		}

		T query(Key_t key) {
			uint32_t pos;
			T result = std::numeric_limits<T>::max();
			for (int i = 0; i < depth; ++i) {
				pos = hash(key, i);
				if (matrix[i][pos].k == key) {
					result = std::min(result, (matrix[i][pos].v + matrix[i][pos].c) / 2);
				}
				else {
					result = std::min(result, (matrix[i][pos].v - matrix[i][pos].c) / 2);
				}
			}

			return result;
		}

		void getHeavyHitters() {
			// std::cout << "Threshold: " << threshold * totPkts << std::endl;
			for (int i = 0; i < depth; ++i) {
				for (int j = 0; j < width; ++j) {
					if (!matrix[i][j].k.isEmpty()) {
						T estimate = query(matrix[i][j].k);
						// if (five_tuple((char*)&testKey) == matrix[i][j].k) {
						// 	std::cout << "MV: " << estimate << std::endl;
						// }
						if (estimate > threshold
							&& this->heavyHitters.find(matrix[i][j].k) == this->heavyHitters.end()) {
								this->heavyHitters.emplace(matrix[i][j].k, estimate);
						}
					}
				}
			}
		}

		// void getSuperspreaders() {}

		void clear() {
			for (int i = 0; i < depth; ++i) {
				for (int j = 0; j < width; ++j) {
					matrix[i][j].v = matrix[i][j].c = 0;
					memset(matrix[i][j].k.str, 0, CHARKEY_LEN);
				}
			}
			totPkts = 0;
			// std::cout << "Clear\n";
		}

		size_t getMemoryUsage() {
			return d * width * (CHARKEY_LEN + 2 * sizeof(T));
		}
};

#endif
