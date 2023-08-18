#ifndef _UTILS_H
#define _UTILS_H

#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <cmath>
#include <limits>
#include <iostream>
#include <algorithm>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <random>
#include <array>
#include <chrono>
#include <numeric>

#include "AwareHash.h"
#include "flowkey.h"

uint8_t test3[13] = {75, 144, 203, 0, 139, 169, 220, 60, 80, 0, 138, 139, 6};
uint8_t test4[13] = {13, 192, 191, 74, 25, 8, 252, 227, 80, 0, 117, 117, 6};

bool isPrime(uint32_t num) {
	uint32_t border = (uint32_t)ceil(sqrt((double)num));
	for (uint32_t i = 2; i <= border; ++i) {
		if ((num % i) == 0)
			return false;
	}
	return true;
}

uint32_t calNextPrime(uint32_t num) {
	while (!isPrime(num)) {
		num++;
	}
	return num;
}

class DataProfiler {
public:
	// std::vector<std::set<Key_t>> heavyHitters;
	// std::vector<std::set<IPKey_t>> superspreaders;
	std::vector<std::vector<data_t>> traces;
	std::map<Key_t, uint32_t> heavyHitters;
	std::map<Key_t, uint32_t> tumbling_hh;
	std::set<IPKey_t> superspreaders;
	std::vector<std::map<Key_t, uint32_t>> flowSizes;
	std::vector<uint32_t> totalPackets;
	std::vector<uint32_t> card;
	std::map<Key_t, bool> lossFlow;
	// std::map<five_tuple, uint32_t> s[5];

	DataProfiler(const char * path, bool loss = false) {
		FILE *inputData = fopen(path, "rb");

		if (inputData == NULL) {
			std::cout << "Input data not found\n";
			exit(0);
		}

		traces.clear();
		char *strData = new char[DATA_T_SIZE];
		double startTime = 0;
		uint32_t total_pkts = 0;
		int cur = 0;
		// uint32_t length = 0;
		double st, ed;

		flowSizes = std::vector<std::map<five_tuple, uint32_t>>(600, std::map<five_tuple, uint32_t>());
		totalPackets = std::vector<uint32_t>(600, 0);

		printf("Reading in data\n");

		traces.push_back(std::vector<data_t>());
		if (fread(strData, DATA_T_SIZE, 1, inputData) == 1) {
			traces.rbegin()->push_back(data_t(strData));
			traces.rbegin()->rbegin()->timestamp = *(double *)(strData+26);
			startTime = traces.rbegin()->rbegin()->timestamp;
			total_pkts += 1;
			flowSizes[cur][traces.rbegin()->rbegin()->key] += traces.rbegin()->rbegin()->length;
			totalPackets[cur] += traces.rbegin()->rbegin()->length;
			// flowSizes[cur][traces.rbegin()->rbegin()->key] += 1;
			// totalPackets[cur] += 1;

			// s[0][traces.rbegin()->rbegin()->key] += 1;
		}

		while (fread(strData, DATA_T_SIZE, 1, inputData) == 1) {
			data_t data(strData);
			data.timestamp = *(double *)(strData+26);
			// length = std::max(data.length, length);
			if (data.timestamp - startTime <= WINDOW_SIZE / MEASURE_POINT) {
			// if (total_pkts < 100000) {
				// s[cur][data.key] += 1;
				traces[cur].push_back(data);
				// if (loss && cur < MEASURE_POINT && lossFlow.size() < 100) {
				// 	if (lossFlow.find(data.key) == lossFlow.end()) {
				// 		lossFlow.emplace(data.key, false);
				// 	}
				// }
			}
			else {
				// break;
				// if (traces.size() == WINDOW_NUM * MEASURE_POINT )
				// 	break;
				traces.push_back(std::vector<data_t>());
				cur++;
				traces[cur].push_back(data);
				startTime = data.timestamp;
			}


			// Key_t testKey = Key_t((char *)test3);
			// if (data.key == testKey) {
			//      std::cout << "hah in reading data: " << data.length << std::endl;
			// }			

			total_pkts += 1;
			for (int k = std::max(0, cur - (MEASURE_POINT - 1)); k <= cur; ++k) {
				flowSizes[k][data.key] += data.length;
				totalPackets[k] += data.length;
				// flowSizes[k][data.key] += 1;
				// totalPackets[k] += 1;
			}
		}
		fclose(inputData);
		
		printf("Successfully read in %d packets (%ld subwindows)\n", total_pkts, traces.size());

		// getFlowSizes();

		getHeavyHitters();

		getCardinality();

		getSuperspreaders();
	}

private:
	void getFlowSizes() {
		int size = traces.size();
		int start = 0;
		for (int i = 0; i < size; ++i) {
			for (auto j = traces[i].begin(); j != traces[i].end(); ++j) {
				for (int k = std::max(0, i - 4); k <= i; ++k) {
					flowSizes[k][j->key] += 1;
					totalPackets[k] += 1;
				}
			}
		}
	}

	void getHeavyHitters() {
		int size = traces.size();
		int start = 0;
		// std::set<Key_t> tumbling_hh;
		for (int i = MEASURE_POINT - 1; i < size; ++i) {
			// heavyHitters.push_back(std::set<Key_t>());
			int current = i + 1 - MEASURE_POINT;
			uint32_t maxlength = 0;
			for (auto j = flowSizes[current].begin(); j != flowSizes[current].end(); ++j)  {
				maxlength = std::max(maxlength, j->second);
				// if (j->second > HH_THRESHOLD * totalPackets[current]) {
				if (j->second > HH_THRESHOLD) {
					// heavyHitters.rbegin()->insert(j->first);
					// std::cout << j->second << " " << totalPackets[current] << std::endl;
					heavyHitters.emplace(j->first, j->second);
					if (current % MEASURE_POINT == 0)
						tumbling_hh.emplace(j->first, j->second);
				}
			}
			// std::cout << " # of hh in tw: " << tumbling_hh.size() << std::endl;
			// std::cout << "Threshold: " << totalPackets[current] * HH_THRESHOLD << std::endl;
			// std::cout << "max: " << maxlength << " " << HH_THRESHOLD * totalPackets[current] << std::endl;
		}
		std::cout << "Recall for HH (ideal tw): " << heavyHitters.size() << " " << tumbling_hh.size() << " " << (double)tumbling_hh.size() / heavyHitters.size() << std::endl;
	}

	void getSuperspreaders() {
		int size = traces.size();
		int start = 0;
		std::set<IPKey_t> tumbling_ss;
		std::map<IPKey_t, std::map<IPKey_t, uint32_t>> IP2card;
		for (int i = 0; i < size; ++i) {
			if (i > MEASURE_POINT - 1) {
				start = i - MEASURE_POINT;
				for (auto j = traces[start].begin(); j != traces[start].end(); ++j) {
					IP2card[*(IPKey_t*)j->key.str][*(IPKey_t*)((char*)j->key.str + 4)] -= 1;
					if (IP2card[*(IPKey_t*)j->key.str][*(IPKey_t*)((char*)j->key.str + 4)] == 0)
						IP2card[*(IPKey_t*)j->key.str].erase(*(IPKey_t*)((char*)j->key.str + 4));
				}
			}

			for (auto j = traces[i].begin(); j != traces[i].end(); ++j) {
				IP2card[*(IPKey_t*)j->key.str][*(IPKey_t*)((char*)j->key.str + 4)] += 1;
			}

			if (i >= MEASURE_POINT - 1) {
				// superspreaders.push_back(std::set<IPKey_t>());
				for (auto j = IP2card.begin(); j != IP2card.end(); ++j) {
					if (j->second.size() > SS_THRESHOLD) {
						// superspreaders.rbegin()->insert(j->first);
						superspreaders.insert(j->first);
						if (i % MEASURE_POINT == MEASURE_POINT - 1)
							tumbling_ss.insert(j->first);
					}
				}
			}
		}

		// std::cout << "IP2card size: " << IP2card.size() << std::endl;

		// for (auto i = IP2card.begin(); i != IP2card.end(); ++i) {
		// 	// for (auto j = i->second.begin(); j != i->second.end(); ++j) {
		// 	// 	std::cout << (uint32_t)i->first << " " << (uint32_t)j->first << " " << j->second << std::endl;
		// 	// }
		// 	if (i->second.size() > SS_THRESHOLD) {
		// 		superspreaders.insert(i->first);
		// 		// std::cout << (uint32_t)i->first << " " << i->second.size() << std::endl;
		// 	}
		// }

		std::cout << "Recall for SS (ideal tw): " << superspreaders.size() << " " << tumbling_ss.size() << " " << (double)tumbling_ss.size() / superspreaders.size() << std::endl;
	}

	void getCardinality() {
		int size = traces.size();
		int start = 0;
		std::map<five_tuple, uint32_t> IP2card;
		for (int i = 0; i < size; ++i) {
			// if (i > MEASURE_POINT - 1) {
			// 	start = i - MEASURE_POINT;
			// 	for (auto j = traces[start].begin(); j != traces[start].end(); ++j) {
			// 		IP2card[j->key] -= 1;
			// 		if (IP2card[j->key] == 0)
			// 			IP2card.erase(j->key);
			// 	}
			// }

			// for (auto j = traces[i].begin(); j != traces[i].end(); ++j) {
			// 	IP2card[j->key]+= 1;
			// }

			if (i >= MEASURE_POINT - 1) {
				card.push_back(flowSizes[i + 1 - MEASURE_POINT].size());
			}
		}
	}
};

#endif
