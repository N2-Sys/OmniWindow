#ifndef _SL_MV_SKETCH_H
#define _SL_MV_SKETCH_H

#include "MVSketch.h"

template<typename T, uint32_t d, uint32_t w>
class SlidingMV:public SketchBase<T>  {
	public:
		MVSketch<T, d, w> mvNew;
		MVSketch<T, d, w> mvOld;
		int size = mvNew.width * d;
        int head_i = 0, head_j = 0;
		double threshold;

		SlidingMV(double thld):threshold(thld) {
            this->name = "MV";
			mvNew = MVSketch<T, d, w>(threshold);
			mvOld = MVSketch<T, d, w>(threshold);
        }

		void insert(Key_t key, T val = 1, bool scanOrNot = false) {
            mvNew.insert(key, val);
            if (scanOrNot)
                scan();
        }

        T query(Key_t key) {
			uint32_t pos;
			T result = std::numeric_limits<T>::max();
			for (int i = 0; i < d; ++i) {
				pos = mvNew.hash(key, i);
				T re;
				if (mvNew.matrix[i][pos].k == key) {
					re = (mvNew.matrix[i][pos].v + mvNew.matrix[i][pos].c) / 2;
				}
				else {
					re = (mvNew.matrix[i][pos].v - mvNew.matrix[i][pos].c) / 2;
				}

				if (mvOld.matrix[i][pos].k == key) {
					re += (mvOld.matrix[i][pos].v + mvOld.matrix[i][pos].c) / 2;
				}
				else {
					re += (mvOld.matrix[i][pos].v - mvOld.matrix[i][pos].c) / 2;
				}

				result = std::min(result, re);
			}

			return result;
		}

		inline void scan() {
			int cnt = size / MEASURE_POINT;
            bool stop = false;
            for (; head_i < d; ++head_i) {
                for (; head_j < mvNew.width; ++head_j) {
                    mvOld.matrix[head_i][head_j] = mvNew.matrix[head_i][head_j];
                    mvNew.matrix[head_i][head_j].c = mvNew.matrix[head_i][head_j].v = 0;
                    mvNew.matrix[head_i][head_j].k = five_tuple();
                    cnt--;
                    if (!cnt)
                        stop = true;
                }
                if (stop)
                    break;
            }

            if (head_i == d && head_j == mvNew.width) {
				mvOld.totPkts = mvNew.totPkts;
				mvNew.totPkts = 0;
				head_j = head_i = 0;
			}
		}

		void getHeavyHitters() {
			// std::cout << "hi mv\n";
			for (int i = 0; i < d; ++i) {
				for (int j = 0; j < mvNew.width; ++j) {
					if (!mvNew.matrix[i][j].k.isEmpty()) {
						T estimate = query(mvNew.matrix[i][j].k);
						// if (estimate > threshold * (mvNew.totPkts + mvOld.totPkts)
						if (estimate > threshold
							&& this->heavyHitters.find(mvNew.matrix[i][j].k) == this->heavyHitters.end()) {
								this->heavyHitters.emplace(mvNew.matrix[i][j].k, estimate);
						}
					}
					if (!mvOld.matrix[i][j].k.isEmpty()) {
						T estimate = query(mvOld.matrix[i][j].k);
						// if (estimate > threshold * (mvNew.totPkts + mvOld.totPkts)
						if (estimate > threshold
							&& this->heavyHitters.find(mvOld.matrix[i][j].k) == this->heavyHitters.end()) {
								this->heavyHitters.emplace(mvOld.matrix[i][j].k, estimate);
						}
					}
				}
			}
		}
};

#endif
