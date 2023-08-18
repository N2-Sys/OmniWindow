#ifndef _SLIDING_SKETCH_H
#define _SLIDING_SKETCH_H

#include "sketchBase.h"


template<typename T>
class SlidingSketch {
	SketchBase<T> *sketch;
	DataProfiler * data;


	std::vector<double> FSARE;
	std::vector<double> CDRE;


	double avgFSARE, avgCDRE;
	double avgHHPrecision, avgHHRecall, avgHHF1;
	double avgSSPrecision, avgSSRecall, avgSSF1;
	uint32_t total_packets;

public:
	SlidingSketch(SketchBase<T> *sk, DataProfiler *dp):sketch(sk), data(dp) {}

	void insertAll() {
		int size = data->traces.size();
		double startTime;
		for (int i = 0; i < size; ++i) {
			std::vector<data_t> &t = data->traces[i];

			bool scanOrNot = false;

			// if (i >= MEASURE_POINT)
				scanOrNot = true;

			for (auto j = t.begin(); j != t.end(); ++j) {
				if (sketch->name == "SS" || sketch->name == "VBF") {
					sketch->insert_ss((*(IPKey_t*)(j->key.str)), (*(IPKey_t*)((char *)j->key.str + 4)), scanOrNot);
				}
				else
					sketch->insert(j->key, j->length, scanOrNot);
				if (scanOrNot)
					scanOrNot = false;
				total_packets++;
			}

			if (i >= MEASURE_POINT - 1) {
				if (sketch->name == "CM" || sketch->name == "SM")
					flowSizeEst(i + 1 - MEASURE_POINT);
				if (sketch->name == "LC" || sketch->name == "HLL")
					cardinalityEst(i + 1 - MEASURE_POINT);
				if (sketch->name == "MV" || sketch->name == "HP")
					sketch->getHeavyHitters();
				if (sketch->name == "SS" || sketch->name == "VBF")
					sketch->getSuperspreaders();
				flowSizeEst(i + 1 - MEASURE_POINT);
			}
		}
		if (sketch->name == "MV" || sketch->name == "HP")
			heavyHitterEst();
		if (sketch->name == "SS" || sketch->name == "VBF")
			superspreaderEst();
	}

	void flowSizeEst(int windowNum) {
		double are = 0;
		std::map<Key_t, uint32_t> & t = data->flowSizes[windowNum];
		for (auto i = t.begin(); i != t.end(); ++i) {
			uint32_t est = sketch->query(i->first);
			are += fabs((double)est - (double)(i->second)) / i->second;
		}
		are /= t.size();
		FSARE.push_back(are);
	}

	void cardinalityEst(int windowNum) {
		uint32_t c = sketch->getCardinality();
		double re = fabs((double)c - (double)data->card[windowNum]) / data->card[windowNum];
		CDRE.push_back(re);
	}

	void heavyHitterEst() {
		double tp =0, fp = 0, fn = 0;
		std::map<Key_t, uint32_t> &t = data->heavyHitters;
		for (auto i = sketch->heavyHitters.begin(); i != sketch->heavyHitters.end(); ++i) {
			if (t.find(i->first) != t.end()) {
				tp++;
			}
			else {
				fp++;
			}
		}
		for (auto i = t.begin(); i != t.end(); ++i) {
			if (sketch->heavyHitters.find(i->first) == sketch->heavyHitters.end()) {
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
		for (auto i = sketch->superspreaders.begin(); i != sketch->superspreaders.end(); ++i) {
			if (t.find(i->first) != t.end()) {
				tp++;
			}
			else {
				fp++;
			}
		}
		for (auto i = t.begin(); i != t.end(); ++i) {
			if (sketch->superspreaders.find(*i) == sketch->superspreaders.end()) {
				fn++;
			}
		}
		double pr = (double)tp / (tp + fp);
		double rc = (double)tp / (tp + fn);
		avgSSPrecision = pr;
		avgSSRecall = rc;
		avgSSF1 = 2 * pr * rc / (pr + rc);
	}

	void printResults() {
		std::cout << sketch->name << std::endl;
		if (sketch->name == "CM" || sketch->name == "SM") {
			avgFSARE = std::accumulate(FSARE.begin(), FSARE.end(), 0.000) / FSARE.size();
			std::cout << "Average ARE for flow size: " << avgFSARE << std::endl;
		}
		else if (sketch->name == "LC" || sketch->name == "HLL") {
			avgCDRE = std::accumulate(CDRE.begin(), CDRE.end(), 0.000) / CDRE.size();
			std::cout << "Average RE for cardinality: " << avgCDRE << std::endl;
		}
		else if (sketch->name == "MV" || sketch->name == "HP") {
			std::cout << "Average Precision for HH: " << avgHHPrecision << std::endl;
			std::cout << "Average Recall for HH: " << avgHHRecall << std::endl;
		}
		else if (sketch->name == "SS" || sketch->name == "VBF") {
			std::cout << "Average Precision for SS: " << avgSSPrecision << std::endl;
			std::cout << "Average Recall for SS: " << avgSSRecall << std::endl;
		}
	}
};

#endif
