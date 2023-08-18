#ifndef _VECTOR_BF_H
#define _VECTOR_BF_H

#define array_num 5
#define d 8192
#define e 2.71828

template<typename T, uint32_t w>
class VectorBF:public SketchBase<T> {
public:
	uint32_t width = calNextPrime(w);
	uint32_t byte_num;
	uint8_t *A[array_num][d];
	uint64_t h, s, n;
	double delta;
	uint32_t z;
	uint32_t threshold;

	std::set<IPKey_t> keySet;

		VectorBF(uint32_t thld):threshold(thld) {
			byte_num = width / 8 + (width % 8 != 0);
			A[0][0] = new uint8_t[array_num * d * byte_num];
			for (int i = 0; i < array_num; ++i) {
				for (int j = 0; j < d; ++j) {
					if (i != 0 || j != 0) {
						A[i][j] = A[0][0] + (i * d + j) * byte_num;
					}
				}
			}
			memset(A[0][0], 0, array_num * d * byte_num);
			int index = 0;
			h = GenHashSeed(index++);
			s = GenHashSeed(index++);
			n = GenHashSeed(index++);
			this->superspreaders.clear();
			keySet.clear();
			this->name = std::string("VBF");
		}

		inline uint32_t hash_h(IPKey_t k, int i) {
			if (i < 3) {
				k = k << (i * 8);
				return (k >> 20) % d;
			}
			else if (i == 3)
				return (((k & 0xff) << 4) | (k >> 28)) % d;
			else
				return (uint32_t)(AwareHash((unsigned char *)&k, 4, h, s, n) % d);
		}

		inline uint32_t hash_f(IPKey_t k, IPKey_t dst) {
			uint64_t tmp_key = ((uint64_t)k << 32) | (uint32_t)dst;
			return (uint32_t)(AwareHash((unsigned char *)(&tmp_key), 8, h, s, n) % width);
		}

		void insert_ss(IPKey_t k, IPKey_t dst, bool scanOrNot = false) {
			uint32_t col_num = hash_f(k, dst);
			for (int i = 0; i < array_num; ++i) {
				uint32_t row_num = hash_h(k, i);
				// std::cout << (uint32_t)A[i][row_num][col_num >> 3] << std::endl;
				A[i][row_num][col_num >> 3] |= (1 << (col_num & 0x7));
				// std::cout << (uint32_t)A[i][row_num][col_num >> 3] << std::endl;
			}
			keySet.insert(k);
		}

		int countZeroBitsInByte(uint8_t x) {
			x = ~x;
			x = (x & 0x55) + ((x >> 1) & 0x55);
			x = (x & 0x33) + ((x >> 2) & 0x33);
			x = (x & 0x0f) + ((x >> 4) & 0x0f);
			return x;
		}

		uint32_t calculateZero(uint8_t *v) {
			int ret = 0;
			for (int i = 0; i < byte_num; ++i) {
				ret += countZeroBitsInByte(v[i]);
			}

			ret -= 8 - (width & 0x7);
			return ret;
		}

		void calculateDelta() {
			uint32_t zero = 0;
			for (int i = 0; i < d; ++i) {
				zero += calculateZero(A[4][i]);
			}
			// std::cout << "width: " << width << std::endl;
			// std::cout << "zero in A[5]: " << zero << std::endl;
			double v = (double)zero / (width * d);
			// std::cout << "v: " << v << std::endl;
			delta = (-1) * (double)width * log(v * (2 - v));
			// std::cout << "Delta: " << delta << std::endl;
		}

		void calculateZ(){
			z = width * pow(e, -(threshold + delta) / (double)width);
			// std::cout << "Z: " << z << std::endl;
		}		

		uint32_t calculateCard(uint32_t zs) {
			return (double)width * log((double)width / zs) - delta;
		}

		void getSuperspreaders() {
			// std::cout << "hi in get ss\n";
			calculateDelta();
			calculateZ();
			uint8_t *resultVec = new uint8_t[byte_num];
			for (auto it = keySet.begin(); it != keySet.end(); ++it) {
				for (int i = 0; i < array_num; ++i) {
					uint32_t row_num = hash_h(*it, i);
					if (i == 0)
						memcpy(resultVec, A[0][row_num], byte_num);
					else
						for (int j = 0; j < byte_num; ++j)
							resultVec[j] &= A[i][row_num][j];
				}
				uint32_t zs = calculateZero(resultVec);
				if (zs < z) {
					uint32_t card = calculateCard(zs);
					this->superspreaders.emplace(*it, card);
				}
				// else {
				// 	std::cout << zs << " " << z << std::endl;
				// }
			}

			// std::cout << "# of detected ss: " << this->superspreaders.size() << std::endl;
			// std::cout << "size of key set: " << keySet.size() << std::endl;

			delete []resultVec;
		}

		void clear() {
			memset(A[0][0], 0, array_num * d * byte_num);
			keySet.clear();
		}

		void operator |= (const VectorBF<T, w> &v) {
			// std::cout << "|=\n";
			for (int i = 0; i < array_num; ++i) {
				for (int j = 0; j < d; ++j) {
					for (int k = 0; k < byte_num; ++k) {
						A[i][j][k] |= v.A[i][j][k];
					}
				}
			}
			for (auto j = v.keySet.begin(); j != v.keySet.end(); ++j) {
				this->keySet.insert(*j);
			}
			// return this;
		}

		size_t getMemoryUsage() {
			return array_num * d * width / 8;
		}
};

#endif
