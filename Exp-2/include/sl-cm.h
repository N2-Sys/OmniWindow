#ifndef _SL_CM_SKETCH_H
#define _SL_CM_SKETCH_H

#include "CMSketch.h"

template<typename T, uint32_t d, uint32_t w>
class SlidingCM:public SketchBase<T> {
    public:
        CMSketch<T, d, w> cmNew{};
        CMSketch<T, d, w> cmOld{};
        int size = cmNew.width * d;
        int head_i = 0, head_j = 0;

        SlidingCM() {
            this->name = "CM";
        }

        void insert(Key_t key, T val = 1, bool scanOrNot = false) {
            cmNew.insert(key, val);
            if (scanOrNot)
                scan();
        }

        T query(Key_t key) {
            uint32_t pos;
            T result = std::numeric_limits<T>::max();
            for (int i = 0; i < d; ++i) {
                pos = cmNew.hash(key, i);
                result = std::min(result, cmNew.matrix[i][pos] + cmOld.matrix[i][pos]);
            }
            return result;
        }

        inline void scan() {
            int cnt = size / MEASURE_POINT + 1;
            bool stop = false;
            for (; head_i < d; ++head_i) {
                for (; head_j < cmNew.width; ++head_j) {
                    cmOld.matrix[head_i][head_j] = cmNew.matrix[head_i][head_j];
                    cmNew.matrix[head_i][head_j] = 0;
                    cnt--;
                    if (!cnt)
                        stop = true;
                }
                if (stop)
                    break;
            }

            if (head_i == d && head_j == cmNew.width)
                head_j = head_i = 0;
        }

};

#endif
