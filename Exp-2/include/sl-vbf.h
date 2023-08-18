#ifndef _SL_VBF_H
#define _SL_VBF_H

#include "VectorBF.h"

template<typename T, uint32_t w>
class SlidingVBF:public SketchBase<T> {
	public:
		VectorBF<T, w> vbfNew{SS_THRESHOLD};
		VectorBF<T, w> vbfOld{SS_THRESHOLD};
		int size = array_num * d;
        int head_i = 0, head_j = 0, head_k = 0;
		std::set<IPKey_t> keySet[MEASURE_POINT];

		SlidingVBF() {
            this->name = "VBF";
        }

        void insert_ss(IPKey_t key, IPKey_t dst, bool scanOrNot = false) {
			// std::cout << "hi\n";
			vbfNew.insert_ss(key, dst);
			keySet[head_k].insert(key);
			if (scanOrNot) {
				head_k = (head_k + 1) % MEASURE_POINT;
				keySet[head_k].clear();
				scan();
			}
		}

		inline void scan() {
			int cnt = size / MEASURE_POINT + 1;
            bool stop = false;
            // vbfOld.keySet = vbfNew.keySet;
            // vbfNew.keySet.clear();
			for (; head_i < array_num; ++head_i) {
				for (; head_j < d; ++head_j) {
					for (int i = 0; i < vbfNew.byte_num; ++i) {
                		vbfOld.A[head_i][head_j][i] = vbfNew.A[head_i][head_j][i];
                		vbfNew.A[head_i][head_j][i] = 0;
                	}
                    cnt--;
                    if (!cnt)
                        stop = true;
                }
                if (stop)
                    break;
            }

            if (head_i == d && head_j == vbfNew.byte_num)
                head_j = head_i = 0;
		}

		void getSuperspreaders() {
			vbfOld |= vbfNew;

			for (int j = 0; j < MEASURE_POINT; ++j) {
				for (auto i = keySet[j].begin(); i != keySet[j].end(); ++i) {
					vbfOld.keySet.insert(*i);
				}
			}

			vbfOld.getSuperspreaders();

			for (auto j = vbfOld.superspreaders.begin(); j != vbfOld.superspreaders.end(); ++j)
				this->superspreaders.emplace(j->first, j->second);

			// for (int i = 0; i < array_num; ++i) {
			// 	for (int j = 0; j < d; ++j) {
			// 		for (int k = 0; k < vbfOld.byte_num; ++k) {
			// 			vbfOld.A[i][j][k] |= vbfNew.A[i][j][k];
			// 		}
			// 	}
			// }

			// vbfOld.calculateDelta();
			// vbfOld.calculateZ();

			// uint8_t *resultVec = new uint8_t[vbfOld.byte_num];
			// for (auto it = vbfNew.keySet.begin(); it != vbfNew.keySet.end(); ++it) {
			// 	// memset(resultVec, 0, vbfOld.byte_num);
			// 	for (int i = 0; i < array_num; ++i) {
			// 		uint32_t row_num = vbfOld.hash_h(*it, i);
			// 		if (i == 0)
			// 			memcpy(resultVec, vbfOld.A[i][row_num], vbfOld.byte_num);
			// 		else
			// 			for (int j = 0; j < vbfOld.byte_num; ++j)
			// 				resultVec[j] &= vbfOld.A[i][row_num][j];
			// 	}
			// 	uint32_t zs = vbfOld.calculateZero(resultVec);
			// 	if (zs < vbfOld.z) {
			// 		uint32_t card = vbfOld.calculateCard(zs);
			// 		this->superspreaders.emplace(*it, card);
			// 	}
			// }

			// for (auto it = vbfOld.keySet.begin(); it != vbfOld.keySet.end(); ++it) {
			// 	memset(resultVec, 0, vbfOld.byte_num);
			// 	for (int i = 0; i < array_num; ++i) {
			// 		uint32_t row_num = vbfOld.hash_h(*it, i);
			// 		for (int j = 0; j < vbfOld.byte_num; ++j)
			// 			resultVec[j] &= vbfOld.A[i][row_num][j];
			// 	}
			// 	uint32_t zs = vbfOld.calculateZero(resultVec);
			// 	if (zs < vbfOld.z) {
			// 		uint32_t card = vbfOld.calculateCard(zs);
			// 		this->superspreaders.emplace(*it, card);
			// 	}
			// }

			// delete []resultVec;
		}
};

#endif
