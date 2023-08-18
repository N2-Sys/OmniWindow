#ifndef _SL_SPREAD_SKETCH_H
#define _SL_SPREAD_SKETCH_H

#include "spreadSketch.h"

template<typename T, uint32_t d, uint32_t w>
class SlidingSS:public SketchBase<T>  {
	public:
		SpreadSketch<T, d, w> ssNew;
		SpreadSketch<T, d, w> ssOld;
		int size = ssNew.width * d;
        int head_i = 0, head_j = 0;

		SlidingSS(uint32_t _b, uint32_t _bb, uint32_t _c, double thld) {
			ssNew = SpreadSketch<T, d, w>(_b, _bb, _c, thld);
			ssOld = SpreadSketch<T, d, w>(_b, _bb, _c, thld);
            this->name = "SS";
        }

		void insert_ss(IPKey_t key, IPKey_t dst, bool scanOrNot) {
			ssNew.insert_ss(key, dst);
			if (scanOrNot)
				scan();
		}

		inline void scan() {
			int cnt = size / MEASURE_POINT;
            bool stop = false;
            for (; head_i < d; ++head_i) {
                for (; head_j < ssNew.width; ++head_j) {
                    ssOld.matrix[head_i][head_j] = ssNew.matrix[head_i][head_j];
                    ssNew.matrix[head_i][head_j].candidate = 0;
                    ssNew.matrix[head_i][head_j].l = 0;
                    ssNew.matrix[head_i][head_j].v.clear();
                    cnt--;
                    if (!cnt)
                        stop = true;
                }
                if (stop)
                    break;
            }

            if (head_i == d && head_j == ssNew.width)
                head_j = head_i = 0;
		}

		void getSuperspreaders() {
			ssOld |= ssNew;
			ssOld.getSuperspreaders();
			for (auto j = ssOld.superspreaders.begin(); j != ssOld.superspreaders.end(); ++j)
				this->superspreaders.emplace(j->first, j->second);

			// std::set<IPKey_t> s;
			// s.clear();
			// for (int i = 0; i < d; ++i) {
			// 	for (int j = 0; j < ssNew.width; ++j) {
			// 		if (ssNew.matrix[i][j].l > ssOld.matrix[i][j].l)
			// 			s.insert(ssNew.matrix[i][j].candidate);
			// 		else if (ssNew.matrix[i][j].l < ssOld.matrix[i][j].l)
			// 			s.insert(ssOld.matrix[i][j].candidate);
			// 		else {
			// 			s.insert(ssNew.matrix[i][j].candidate);
			// 			s.insert(ssOld.matrix[i][j].candidate);
			// 		}
			// 		ssOld.matrix[i][j].v |= ssNew.matrix[i][j].v;
			// 	}
			// }

			// for (auto i = s.begin(); i != s.end(); ++i) {
			// 	if (*i == 0) continue;
			// 	uint32_t res = std::numeric_limits<uint32_t>::max();
			// 	for (int j = 0; j < d; ++j) {
			// 		uint32_t pos = ssOld.hash(*i, j);
			// 		res = std::min(res, ssOld.matrix[j][pos].v.query());
			// 	}
			// 	if (res > SS_ESTIMATE_THRESHOLD)
			// 		this->superspreaders.emplace(*i, res);
			// }
		}
};

#endif