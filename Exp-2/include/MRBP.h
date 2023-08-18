#ifndef _MRBP_H
#define _MRBP_H

#include "utils.h"

const double bitsetratio = 0.93105;

class MRBP {
public:
	std::vector<std::vector<uint8_t>> bitmap;
	uint32_t b, bb, c, setmax;

	MRBP(uint32_t _b, uint32_t _bb, uint32_t _c):b(_b), bb(_bb), c(_c) {
		int size_b = b / 8 + (b % 8 != 0);
		int size_bb = bb / 8 + (bb % 8 != 0);
		for (int i = 0; i < c-1; ++i) {
			bitmap.push_back(std::vector<uint8_t>(size_b, 0));
		}
		bitmap.push_back(std::vector<uint8_t>(size_bb, 0));
		setmax = (uint32_t)(b*bitsetratio+0.5);
	}
	MRBP() {}

	~MRBP() {
	}

	int count_zero(uint32_t x) {		// the lowest zero
		int ret = 0;
	    while ((x & 0x00000001) == 1) {
	        x >>= 1;
	        ret++;
	    }
	    return ret;
	}

	int countzerobitsInByte(int level, int idx) {
		uint8_t x = ~bitmap[level][idx];
		x = (x & 0x55) + ((x >> 1) & 0x55);
		x = (x & 0x33) + ((x >> 2) & 0x33);
		x = (x & 0x0f) + ((x >> 4) & 0x0f);
		return x;
	}

	int countzerobits(int level) {		// the number of zeros in a bitmap
		int ending = (level < c - 1)?b:bb;
		int result = 0;
		ending = ending / 8 + (ending % 8 != 0);
		for (int i = 0; i < ending; ++i) {
			result += countzerobitsInByte(level, i);
		}
		if (level < c - 1 && b % 8)
			result -= (8 - b % 8);
		else if (level == c - 1 && bb % 8)
			result -= (8 - bb % 8);

		return result;
	}

	uint32_t update(uint32_t p) {
		uint32_t level = count_zero(p);
		if (level >= c - 1) {
			level = c - 1;
			// p = ((p * (uint64_t)bb) >> 32) % bb;	// whether to mod?
			p = p % bb;
		}
		else {
			// p = ((p * (uint64_t)b) >> 32) % b;		// whether to mod?
			p = p % b;
		}
		bitmap[level][p >> 3] |= (1 << (p & 0x7));
		return level;
	}

	uint32_t query() {
		int base = 0, z = 0;
		int factor = 1;
		double m = 0;
		for (base = 0; base < c - 1; ++base) {
			z = countzerobits(base);
			if (b - z <= setmax) {
				break;
			}
			else {
				factor *= 2;
			}
		}
		
		int pos = base - 1;
		for (int i = base; i < c - 1; ++i) {
			z = countzerobits(i);
			m += b * (log(b) - log(z));
			if (z == 0 || b - z > setmax) {
				pos = i;
				m = 0;
			}
		}
		factor *= (1 << (pos - base + 1));

		z = countzerobits(c - 1);
		if (z == 0) {
			m += bb * log(bb);
		}
		else {
			m += bb * (log(bb) - log(z));
		}
		// std::cout << factor << " " << m << std::endl;
		return (uint32_t)(factor * m + 0.5);
	}

	void clear() {
		int size_b = b / 8 + (b % 8 != 0);
		int size_bb = bb / 8 + (bb % 8 != 0);
		for (int i = 0; i < c-1; ++i) {
			bitmap[i].clear();
			bitmap[i].resize(size_b, 0);
		}
		bitmap[c-1].clear();
		bitmap[c-1].resize(size_bb, 0);
	}

	void operator |= (const MRBP &v) {
		int size_b = b / 8 + (b % 8 != 0);
		int size_bb = bb / 8 + (bb % 8 != 0);
		for (int i = 0; i < c-1; ++i) {
			for (int j = 0; j < size_b; ++j) {
				bitmap[i][j] |= v.bitmap[i][j];
			}
		}
		for (int j = 0; j < size_bb; ++j) {
			bitmap[c-1][j] |= v.bitmap[c-1][j];
		}
		// return bitmap;
	}

	size_t getMemoryUsage() {
		size_t bitnum = bb + b * (c - 1);
		return bitnum / 8 + (bitnum % 8 != 0);
	}
};

#endif