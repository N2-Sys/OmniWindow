#include "sl-cm.h"
#include "sl-sm.h"
#include "sl-mv.h"
#include "sl-hp.h"
#include "sl-ss.h"
#include "sl-vbf.h"
#include "sl-lc.h"
#include "sl-hll.h"
#include "slidingSketch.h"

// const char * dataPath = "/data/OmniWindow-trace/equinix-nyc-modified-tuples.dirB.20180419-130100.UTC.anon.bin";
std::set<std::string> algorithms = {"CM", "SM", "MV", "HP", "SS", "VBF", "LC", "HLL"};
std::vector<std::string> algorithm_name;

int main (int argc, char **argv) {
	if (argc <= 2) {
		std::cout << "Usage: ./exp_sliding algorithm_name data_path\n";
		return 0;
	}

	for (int i = 1; i < argc - 1; ++i) {
		std::string name = std::string(argv[i]);
		if (name == "all") {
			algorithm_name = std::vector<std::string>{"CM", "SM", "MV", "HP", "SS", "VBF", "LC", "HLL"};
			break;
		}
		else if (algorithms.find(name) == algorithms.end()) {
			std::cout << "Invalid algorithm name: " << name << std::endl;
		}
		else {
			algorithm_name.push_back(name);
		}
	}

	std::string dataPath = std::string(argv[argc - 1]);
	DataProfiler * data = new DataProfiler(dataPath.c_str());

	for (auto it = algorithm_name.begin(); it != algorithm_name.end(); ++it) {
		if (*it == "CM") {
			SlidingSketch<CM_TYPE> slCM(new SlidingCM <CM_TYPE, CM_DEPTH, CM_WIDTH>(), data);
			slCM.insertAll();
			slCM.printResults();
		}
		else if (*it == "SM") {
			SlidingSketch<SM_TYPE> slSM(new SlidingSM <SM_TYPE, SM_DEPTH, SM_WIDTH>(), data);
			slSM.insertAll();
			slSM.printResults();
		}
		else if (*it == "MV") {
			SlidingSketch<MV_TYPE> slMV(new SlidingMV<MV_TYPE, MV_DEPTH, MV_WIDTH>(HH_THRESHOLD), data);
			slMV.insertAll();
			slMV.printResults();
		}
		else if (*it == "HP") {
			SlidingSketch<HP_TYPE> slHP(new SlidingHP<HP_TYPE, HP_DEPTH, HP_WIDTH>(HH_THRESHOLD), data);
			slHP.insertAll();
			slHP.printResults();
		}
		else if (*it == "SS") {
			SlidingSketch<SS_TYPE> slSS(new SlidingSS<SS_TYPE, SS_DEPTH, SS_WIDTH>(SS_B, SS_BB, SS_C, SS_THRESHOLD), data);
			slSS.insertAll();
			slSS.printResults();
		}
		else if (*it == "VBF") {
			SlidingSketch<VBF_TYPE> slVBF(new SlidingVBF<VBF_TYPE, VBF_WIDTH>(), data);
			slVBF.insertAll();
			slVBF.printResults();
		}
		else if (*it == "LC") {
			SlidingSketch<LC_TYPE> slLC(new SlidingLC<LC_TYPE, LC_WIDTH>(), data);
			slLC.insertAll();
			slLC.printResults();
		}
		else if (*it == "HLL") {
			SlidingSketch<HLL_TYPE> slHLL(new SlidingHLL<HLL_TYPE, HLL_WIDTH>(), data);
			slHLL.insertAll();
			slHLL.printResults();
		}
	}

	return 0;
}
