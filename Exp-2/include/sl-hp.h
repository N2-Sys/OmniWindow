#ifndef _SLIDING_HASHPIPE_H
#define _SLIDING_HASHPIPE_H

#include "HashPipe.h"

template<typename T, uint32_t d, uint32_t w>
class SlidingHP:public SketchBase<T> {
	public:
		HashPipe<T, d, w> hpNew;
		HashPipe<T, d, w> hpOld;
		int size = hpNew.width * d;
        int head_i = 0, head_j = 0;
		double threshold;

		SlidingHP(double thld):threshold(thld) {
            this->name = "HP";
			hpNew = HashPipe<T, d, w>(threshold);
			hpOld = HashPipe<T, d, w>(threshold);
        }

        void insert(Key_t key, T val = 1, bool scanOrNot = false) {
        	hpNew.insert(key, val);
            if (scanOrNot)
                scan();
        }

        T query(Key_t key) {
        	uint32_t pos;
			T result = 0;
			for (int i = 0; i < d; ++i) {
				pos = hpNew.hash(key, i);
				if (hpNew.matrix[i][pos].k == key)
					result += hpNew.matrix[i][pos].c;
				else if (hpOld.matrix[i][pos].k == key)
					result += hpOld.matrix[i][pos].c;
			}
			return result;
        }

        inline void scan() {
        	int cnt = size / MEASURE_POINT;
            bool stop = false;
            for (; head_i < d; ++head_i) {
                for (; head_j < hpNew.width; ++head_j) {
                    hpOld.matrix[head_i][head_j] = hpNew.matrix[head_i][head_j];
                    hpNew.matrix[head_i][head_j].c = 0;
                    hpNew.matrix[head_i][head_j].k = five_tuple();
                    cnt--;
                    if (!cnt)
                        stop = true;
                }
                if (stop)
                    break;
            }

            if (head_i == d && head_j == hpNew.width) {
				hpOld.totPkts = hpNew.totPkts;
				hpNew.totPkts = 0;
				head_j = head_i = 0;
			}
        }

        void getHeavyHitters() {
			// std::cout << "hi hp\n";
			for (int i = 0; i < d; ++i) {
				for (int j = 0; j < hpNew.width; ++j) {
					if (!hpNew.matrix[i][j].k.isEmpty()) {
						T estimate = query(hpNew.matrix[i][j].k);
						// if (estimate > threshold * (hpOld.totPkts + hpNew.totPkts)
						if (estimate > threshold
							&& this->heavyHitters.find(hpNew.matrix[i][j].k) == this->heavyHitters.end())
							this->heavyHitters.emplace(hpNew.matrix[i][j].k, estimate);
					}
					if (!hpOld.matrix[i][j].k.isEmpty()) {
						T estimate = query(hpOld.matrix[i][j].k);
						// if (estimate > threshold * (hpOld.totPkts + hpNew.totPkts)
						if (estimate > threshold
							&& this->heavyHitters.find(hpOld.matrix[i][j].k) == this->heavyHitters.end())
							this->heavyHitters.emplace(hpOld.matrix[i][j].k, estimate);
					}
				}
			}
		}

};

#endif
