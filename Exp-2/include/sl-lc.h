#ifndef _SL_LINEAR_COUNTING_H
#define _SL_LINEAR_COUNTING_H

#include "LinearCounting.h"

template<typename T, uint32_t w>
class SlidingLC:public SketchBase<T>  {
	public:
		LinearCounting<T, w> lcNew{};
		LinearCounting<T, w> lcOld{};
		int size = lcNew.byte_num;
        int head_i = 0;

		SlidingLC() {
            this->name = "LC";
        }

        void insert(Key_t key, T val = 1, bool scanOrNot = false) {
			lcNew.insert(key, val);
			if (scanOrNot)
				scan();
		}

		inline void scan() {
			int cnt = size / MEASURE_POINT;
			for (; head_i < lcNew.byte_num; ++head_i) {
				lcOld.A[head_i] = lcNew.A[head_i];
				lcNew.A[head_i] = 0;
				cnt--;
                if (!cnt)
                    break;
			}
		}

		uint32_t getCardinality() {
			LinearCounting<T, w> tmp = LinearCounting<T, w>();
			for (int i = 0; i < lcNew.byte_num; ++i) {
				tmp.A[i] = lcOld.A[i] | lcNew.A[i];
			}
			// std::cout << std::endl;
			double zero = tmp.calculateZero(tmp.A);
			// std::cout << "LC: " << lcNew.width << " " << (uint32_t)zero << std::endl;
			return (double)tmp.width * log(zero / tmp.width) * (-1);
		}

};

#endif
