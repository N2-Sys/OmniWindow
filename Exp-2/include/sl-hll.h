#ifndef _SL_HYPER_LOG_LOG_H
#define _SL_HYPER_LOG_LOG_H

#include "hyperLogLog.h"

template<typename T, uint32_t w>
class SlidingHLL:public SketchBase<T> {
	public:
		HyperLL<T, w> hllNew{};
		HyperLL<T, w> hllOld{};
		int size = w;
		int head_i = 0;

		SlidingHLL() {
            this->name = "HLL";
        }

		void insert(Key_t key, T val = 1, bool scanOrNot = false) {
			hllNew.insert(key, val);
			if (scanOrNot)
				scan();
		}

		inline void scan() {
			int cnt = size / MEASURE_POINT;
			// std::cout << "Scan: " << cnt << std::endl;
			for (; head_i < hllNew.width; ++head_i) {
				hllOld.A[head_i] = hllNew.A[head_i];
				hllNew.A[head_i] = 0;
				cnt--;
                if (!cnt)
                    break;
			}
		}

		uint32_t getCardinality() {
			// HyperLL<T, w> tmp = HyperLL<T, w>();
			for (int i = 0; i < hllNew.width; ++i) {
				hllOld.A[i] = std::max(hllNew.A[i], hllOld.A[i]);
			}
			return hllOld.getCardinality();
		}
};

#endif
