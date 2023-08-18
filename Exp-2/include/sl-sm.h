#ifndef _SL_SM_SKETCH_H
#define _SL_SM_SKETCH_H

#include "SuMax.h"

template<typename T, uint32_t d, uint32_t w>
class SlidingSM:public SketchBase<T> {
    public:
        SuMaxSketch<T, d, w> smNew{};
        SuMaxSketch<T, d, w> smOld{};
        int size = smNew.width * d;
        int head_i = 0, head_j = 0;

        SlidingSM() {
            this->name = "SM";
        }

        void insert(Key_t key, T val = 1, bool scanOrNot = false) {
            smNew.insert(key, val);
            if (scanOrNot)
                scan();
        }

        T query(Key_t key) {
            uint32_t pos;
            T result = std::numeric_limits<T>::max();
            for (int i = 0; i < d; ++i) {
                pos = smNew.hash(key, i);
                result = std::min(result, smNew.matrix[i][pos] + smOld.matrix[i][pos]);
            }
            return result;
        }

        inline void scan() {
            int cnt = size / MEASURE_POINT;
            bool stop = false;
            for (; head_i < d; ++head_i) {
                for (; head_j < smNew.width; ++head_j) {
                    smOld.matrix[head_i][head_j] = smNew.matrix[head_i][head_j];
                    smNew.matrix[head_i][head_j] = 0;
                    cnt--;
                    if (!cnt)
                        stop = true;
                }
                if (stop)
                    break;
            }

            if (head_i == d && head_j == smNew.width)
                head_j = head_i = 0;
        }

};

#endif
