#include "CMSketch.h"
#include "SuMax.h"
#include "HashPipe.h"
#include "MVSketch.h"
#include "spreadSketch.h"
#include "VectorBF.h"
#include "LinearCounting.h"
#include "hyperLogLog.h"
#include "tumblingWindowSketch.h"

// const char * dataPath = "/data/OmniWindow-trace/equinix-nyc-modified-tuples.dirB.20180419-130100.UTC.anon.bin";
std::set<std::string> algorithms = {"CM", "SM", "MV", "HP", "SS", "VBF", "LC", "HLL"};
std::vector<std::string> algorithm_name;

int main(int argc, char **argv) {
	if (argc <= 2) {
		std::cout << "Usage: ./exp_tumbling algorithm_name data_path\n";
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
	DataProfiler *data = new DataProfiler(dataPath.c_str());

	for (auto it = algorithm_name.begin(); it != algorithm_name.end(); ++it) {
		if (*it == "CM") {
			TumblingWindowSketch<CM_TYPE> twCM(new CMSketch<CM_TYPE, CM_DEPTH, CM_WIDTH>(), data);
			twCM.insertAll();
			twCM.printResults();
		}
		else if (*it == "SM") {
			TumblingWindowSketch<SM_TYPE> twSM(new SuMaxSketch<SM_TYPE, SM_DEPTH, SM_WIDTH>(), data);
			twSM.insertAll();
			twSM.printResults();
		}
		else if (*it == "MV") {
			TumblingWindowSketch<MV_TYPE> twMV(new MVSketch<MV_TYPE, MV_DEPTH, MV_WIDTH>(HH_THRESHOLD), data);
			twMV.insertAll();
			twMV.printResults();
		}
		else if (*it == "HP") {
			TumblingWindowSketch<HP_TYPE> twHP(new HashPipe<HP_TYPE, HP_DEPTH, HP_WIDTH>(HH_THRESHOLD), data);
			twHP.insertAll();
			twHP.printResults();
		}
		else if (*it == "SS") {
			TumblingWindowSketch<SS_TYPE> twSS(new SpreadSketch<SS_TYPE, SS_DEPTH, SS_WIDTH>(SS_B, SS_BB, SS_C, SS_THRESHOLD), data);
			twSS.insertAll();
			twSS.printResults();
		}
		else if (*it == "VBF") {
			TumblingWindowSketch<VBF_TYPE> twVBF(new VectorBF<VBF_TYPE, VBF_WIDTH>(SS_THRESHOLD), data);
			twVBF.insertAll();
			twVBF.printResults();
		}
		else if (*it == "LC") {
			TumblingWindowSketch<LC_TYPE> twLC(new LinearCounting<LC_TYPE, LC_WIDTH>(), data);
			twLC.insertAll();
			twLC.printResults();
		}
		else if (*it == "HLL") {
			TumblingWindowSketch<HLL_TYPE> twHLL(new HyperLL<HLL_TYPE, HLL_WIDTH>(), data);
			twHLL.insertAll();
			twHLL.printResults();
		}
	}

	return 0;
}
