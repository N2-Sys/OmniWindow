#ifndef _LINEAR_COUNTING_H
#define _LINEAR_COUNTING_H

#include "sketchBase.h"

template<typename T, uint32_t w>
class LinearCounting:public SketchBase<T> {
public:
	uint32_t width = calNextPrime(w);
	uint32_t byte_num;
	uint8_t *A;
	uint64_t h, s, n;

	LinearCounting() {
		byte_num = width / 8 + (width % 8 != 0);
		A = new uint8_t[byte_num];
		memset(A, 0, byte_num);

		int index = 0;
		h = GenHashSeed(index++);
		s = GenHashSeed(index++);
		n = GenHashSeed(index++);

		this->name = std::string("LC");
	}

	inline uint32_t hash(Key_t key) {
		return (uint32_t)(AwareHash((unsigned char *)key.str, CHARKEY_LEN, h, s, n) % width);
	}

	void insert(Key_t key, T val = 1, bool scanOrNot = false) {
		uint32_t pos = hash(key);
		A[pos >> 3] |= (1 << (pos & 0x7));
	}

	int countZeroBitsInByte(uint8_t x) {
		x = ~x;
		x = (x & 0x55) + ((x >> 1) & 0x55);
		x = (x & 0x33) + ((x >> 2) & 0x33);
		x = (x & 0x0f) + ((x >> 4) & 0x0f);
		return x;
	}

	uint32_t calculateZero(uint8_t *v) {
		uint32_t ret = 0;
		for (int i = 0; i < byte_num; ++i) {
			ret += countZeroBitsInByte(v[i]);
		}

		ret -= 8 - (width & 0x7);
		return ret;
	}

	void clear() {
		memset(A, 0, byte_num);
	}

	void operator |= (const LinearCounting<T, w> &v) const {
		for (int i = 0; i < byte_num; ++i) {
			A[i] |= v.A[i];
		}
		// return this;
	}

	uint32_t getCardinality() {
		double zero = calculateZero(A);
		// std::cout << "LC: " << width << " " << (uint32_t)zero << std::endl;
		// std::cout << log(zero / width) << std::endl;
		return (double)width * log(zero / width) * (-1);
	}

	size_t getMemoryUsage() {
		return byte_num;
	}
};

#endif