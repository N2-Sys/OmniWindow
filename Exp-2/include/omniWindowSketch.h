#ifndef _OMNI_WINDOW_SKETCH_H
#define _OMNI_WINDOW_SKETCH_H

#include "sketchBase.h"

template<typename T>
class OmniWindowSketch {
	std::vector<SketchBase<T> *> sketch;
	DataProfiler * data;
	std::set<Key_t> estHeavyHitters;
	std::set<IPKey_t> estSuperspreaders;
	std::set<Key_t> trackedKeys[MEASURE_POINT];

	std::vector<double> FSARE;
	std::vector<double> CDRE;

	double avgFSARE, avgCDRE;
	double avgHHPrecision, avgHHRecall, avgHHF1;
	double avgSSPrecision, avgSSRecall, avgSSF1;
	uint32_t total_packets;
	int head = 0;

public:
	OmniWindowSketch(std::vector<SketchBase<T> *>sk, DataProfiler *dp):sketch(sk), data(dp) {
		// for (auto it = sk.begin(); it != sk.end(); ++it) {
		// 	(*it)->clear();
		// }
	}

	void insertAll() {
		int size = data->traces.size();
		double startTime;
		for (int i = 0; i < size; ++i) {
			std::vector<data_t> &t = data->traces[i];
			if (i != 0)
				head = (head + 1) % MEASURE_POINT;

			sketch[head]->clear();
			sketch[head]->heavyHitters.clear();
			sketch[head]->superspreaders.clear();
			trackedKeys[head].clear();

			for (auto j = t.begin(); j != t.end(); ++j) {
				if (sketch[head]->name == "SS" || sketch[head]->name == "VBF")
					sketch[head]->insert_ss(*(IPKey_t*)j->key.str, *(IPKey_t*)((char *)j->key.str + 4));
				else
					sketch[head]->insert(j->key, j->length);
				total_packets += j->length;
				trackedKeys[head].insert(j->key);
			}

			sketch[head]->getHeavyHitters();

			if (i % MEASURE_POINT == MEASURE_POINT - 1) {
			// if (i >= MEASURE_POINT - 1) {
				if (sketch[0]->name == "CM" || sketch[0]->name == "SM")
					flowSizeEst(i + 1 - MEASURE_POINT);
				if (sketch[0]->name == "LC" || sketch[0]->name == "HLL")
					cardinalityEst(i + 1 - MEASURE_POINT);
				if (sketch[0]->name == "MV" || sketch[0]->name == "HP") {
					heavyHittersMerge(i + 1 - MEASURE_POINT);
					heavyHitterEst();
				}
				if (sketch[0]->name == "SS" || sketch[0]->name == "VBF") {
					superspreadersMerge();
				}
					
			}
		}
	}

	void flowSizeEst(int windowNum) {
		double are = 0;
		std::map<Key_t, uint32_t> & t = data->flowSizes[windowNum];
		uint32_t cnt = 0;
		for (auto i = t.begin(); i != t.end(); ++i) {
			uint32_t est = 0;
			for (int j = 0; j < MEASURE_POINT; ++j) {
				if (trackedKeys[j].find(i->first) != trackedKeys[j].end()) {
					uint32_t r = sketch[j]->query(i->first);
					est += r;
				}
			}

			if (est < i->second) {
				std::cout << est << " " << i->second << std::endl;
			}
			are += fabs((double)est - (double)(i->second)) / i->second;
		}
		are /= t.size();
		FSARE.push_back(are);
	}

	void heavyHittersMerge(int windowNum) {
		// Merge
		int old = (head + 1) % MEASURE_POINT;
		for (int i = (old + 1) % MEASURE_POINT; i != old; i = (i + 1) % MEASURE_POINT) {
			for (auto it = sketch[i]->heavyHitters.begin(); it != sketch[i]->heavyHitters.end(); ++it) {
				if (sketch[old]->heavyHitters.find(it->first) != sketch[old]->heavyHitters.end())
					sketch[old]->heavyHitters[it->first] += it->second;
				else
					sketch[old]->heavyHitters[it->first] = it->second;
			}
		}
		// Detect
		for (auto i = sketch[old]->heavyHitters.begin(); i != sketch[old]->heavyHitters.end(); ++i) {
			// if (i->second > data->totalPackets[windowNum] * HH_THRESHOLD) {
			if (i->second > HH_THRESHOLD) {
				estHeavyHitters.insert(i->first);
			}
		}
	}

	void superspreadersMerge() {
		// Merge
		int old = (head + 1) % MEASURE_POINT;
		for (int i = 0; i < MEASURE_POINT; ++i)
			sketch[i]->getSuperspreaders();
		for (int i = (old + 1) % MEASURE_POINT; i != old; i = (i + 1) % MEASURE_POINT) {
			for (auto j = sketch[i]->superspreaders.begin(); j != sketch[i]->superspreaders.end(); ++j) {
				sketch[old]->superspreaders[j->first] += j->second;
			}
		}
		// Detect
		for (auto i = sketch[old]->superspreaders.begin(); i != sketch[old]->superspreaders.end(); ++i) {
			if (i->second > SS_THRESHOLD)
				estSuperspreaders.insert(i->first);
		}
	}

	void heavyHitterEst() {
		double tp =0, fp = 0, fn = 0;
		std::map<Key_t, uint32_t> &t = data->heavyHitters;
		for (auto i = estHeavyHitters.begin(); i != estHeavyHitters.end(); ++i) {
			if (t.find(*i) != t.end()) {
				tp++;
			}
			else {
				fp++;
			}
		}
		for (auto i = t.begin(); i != t.end(); ++i) {
			if (estHeavyHitters.find(i->first) == estHeavyHitters.end()) {
				fn++;
			}
		}

		double pr = (double)tp / (tp + fp);
		double rc = (double)tp / (tp + fn);
		avgHHPrecision = pr;
		avgHHRecall = rc;
		avgHHF1 = 2 * pr * rc / (pr + rc);
	}

	void superspreaderEst() {
		double tp =0, fp = 0, fn = 0;
		std::set<IPKey_t> &t = data->superspreaders;
		for (auto i = estSuperspreaders.begin(); i != estSuperspreaders.end(); ++i) {
			if (t.find(*i) != t.end()) {
				tp++;
			}
			else {
				fp++;
			}
		}
		for (auto i = t.begin(); i != t.end(); ++i) {
			if (estSuperspreaders.find(*i) == estSuperspreaders.end()) {
				fn++;
			}
		}
		double pr = (double)tp / (tp + fp);
		double rc = (double)tp / (tp + fn);
		avgSSPrecision = pr;
		avgSSRecall = rc;
		avgSSF1 = 2 * pr * rc / (pr + rc);
	}

	void cardinalityEst(int windowNum) {
		uint32_t c = 0;
		uint32_t old = (head + 1) % MEASURE_POINT;
		for (int j = 0; j < MEASURE_POINT; ++j)
			if (sketch[0]->name == "LC" && j != old)
				*(LinearCounting<T, LC_WIDTH>*)sketch[old] |= *(LinearCounting<T, LC_WIDTH>*)sketch[j];
			else if (sketch[0]->name == "HLL" && j != old)
				*(HyperLL<T, HLL_WIDTH>*)sketch[old] |= *(HyperLL<T, HLL_WIDTH>*)sketch[j];
		double re = fabs((double)sketch[old]->getCardinality() - (double)data->card[windowNum]) / data->card[windowNum];
		CDRE.push_back(re);
	}

	void printResults() {
		std::cout << sketch[0]->name << std::endl;
		if (sketch[0]->name == "CM" || sketch[0]->name == "SM") {
			avgFSARE = std::accumulate(FSARE.begin(), FSARE.end(), 0.000) / FSARE.size();
			std::cout << "Average ARE for flow size: " << avgFSARE << std::endl;
		}
		else if (sketch[0]->name == "LC" || sketch[0]->name == "HLL") {
			avgCDRE = std::accumulate(CDRE.begin(), CDRE.end(), 0.000) / CDRE.size();
			std::cout << "Average RE for cardinality: " << avgCDRE << std::endl;
		}
		else if (sketch[0]->name == "MV" || sketch[0]->name == "HP") {
			std::cout << "Average Precision for HH: " << avgHHPrecision << std::endl;
			std::cout << "Average Recall for HH: " << avgHHRecall << std::endl;
		}
		else if (sketch[0]->name == "SS" || sketch[0]->name == "VBF") {
			std::cout << "Average Precision for SS: " << avgSSPrecision << std::endl;
			std::cout << "Average Recall for SS: " << avgSSRecall << std::endl;
		}
	}
};

#endif
