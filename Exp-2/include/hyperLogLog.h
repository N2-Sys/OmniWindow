#ifndef _HYPER_LOG_LOG_H
#define _HYPER_LOG_LOG_H

template<typename T, uint32_t w>
class HyperLL:public SketchBase<T> {
	public:
		T * A;
		uint32_t width = calNextPrime(w);
		uint64_t h[2], s[2], n[2];
		double alpha;

		HyperLL() {
			A = new T[width];
			memset(A, 0, width * sizeof(T));
			int index = 0;
			for (int i = 0; i < 2; ++i) {
				h[i] = GenHashSeed(index++);
				s[i] = GenHashSeed(index++);
				n[i] = GenHashSeed(index++);
			}

			alpha = 0.7213 / (1 + 1.079 / (double)width);
			
			this->name = std::string("HLL");
		}

		inline uint32_t hash(Key_t key, int i) {
			return (uint32_t)(AwareHash((unsigned char *)key.str, CHARKEY_LEN, h[i], s[i], n[i]) % width);
		}

		uint32_t findOne(uint32_t x) {
			uint32_t ret = 1;
			while ((x & 1) == 0) {
				ret++;
				x = x >> 1;
			}
			return ret;
		}

		void insert(Key_t key, T val = 1, bool scanOrNot = false) {
			uint32_t pos = hash(key, 0);
			// uint32_t one = findOne(pos / width);
			uint32_t one = findOne(hash(key, 1));
			A[pos] = std::max(A[pos], (uint8_t)one);
		}

		uint32_t getCardinality() {
			uint32_t cnt = 0;
			double avg = 0;
			for (int i = 0; i < width; ++i) {
				if (A[i] == 0)
					cnt++;
					// std::cout << A[i] << std::endl;
				avg += pow(0.5, A[i]);
			}
			// std::cout << cnt << " " << width << std::endl;
			// std::cout << width << " " << avg << " " << (double)width * width / avg << std::endl;
			double DV_est = alpha * (double)width * width / avg;
			double DV;
			if (DV_est < (double)5 / 2 * width) { // 小范围修正
    			if (cnt == 0)  { // 如果都不是0，无法使用Linear Counting
          			DV = DV_est;
					// std::cout << "1" << std::endl;
				}
    			else {
					DV = (double)width * log((double)width / cnt);	// Linear Counting
					// std::cout << "HLL: " << width << " " << cnt << std::endl;
				}
			}
			else if (DV_est <= ( (double)1 / 30 * pow(2, 32) ))	{ //不用修正
				DV = DV_est;
				// std::cout << "3" << std::endl;
			}
			else if (DV_est > ( (double)1 / 30 * pow(2, 32) )) {	//大范围修正
     			DV = -1 * pow(2, 32) * log(1 - DV_est / pow(2, 32));
				// std::cout << "4" << std::endl;
			}
			return DV;
		}

		void operator |= (const HyperLL<T, w> &v) const {
			for (int i = 0; i < width; ++i) {
				A[i] = std::max(A[i], v.A[i]);
			}
			// return this;
		}

		void clear() {
			memset(A, 0, width * sizeof(T));
		}

		size_t getMemoryUsage() {
			return width * sizeof(T);
		}
};

#endif