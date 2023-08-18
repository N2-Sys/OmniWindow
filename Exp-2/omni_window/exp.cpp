#include "CMSketch.h"
// #include "CUSketch.h"
#include "SuMax.h"
#include "HashPipe.h"
#include "MVSketch.h"
#include "spreadSketch.h"
#include "VectorBF.h"
#include "LinearCounting.h"
#include "hyperLogLog.h"
#include "omniWindowSketch.h"


// const char * dataPath = "/data/OmniWindow-trace/equinix-nyc-modified-tuples.dirB.20180419-130100.UTC.anon.bin";
std::set<std::string> algorithms = {"CM", "SM", "MV", "HP", "SS", "VBF", "LC", "HLL"};
std::vector<std::string> algorithm_name;

int main(int argc, char **argv) {
	if (argc <= 2) {
		std::cout << "Usage: ./exp_omni algorithm_name data_path\n";
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
			std::vector<SketchBase<CM_TYPE> *> sketchVectorCM;
			for (int i = 0; i < MEASURE_POINT; ++i) {
				sketchVectorCM.push_back(new CMSketch<CM_TYPE, CM_DEPTH, CM_WIDTH>());
			}
			OmniWindowSketch<CM_TYPE> omCM(sketchVectorCM, data);
			omCM.insertAll();
			omCM.printResults();
		}
		else if (*it == "SM") {
			std::vector<SketchBase<SM_TYPE> *> sketchVectorSM;
			for (int i = 0; i < MEASURE_POINT; ++i) {
				sketchVectorSM.push_back(new SuMaxSketch<SM_TYPE, SM_DEPTH, SM_WIDTH>());
			}
			OmniWindowSketch<SM_TYPE> omSM(sketchVectorSM, data);
			omSM.insertAll();
			omSM.printResults();
		}
		else if (*it == "MV") {
			std::vector<SketchBase<MV_TYPE> *> sketchVectorMV;
			for (int i = 0; i < MEASURE_POINT; ++i) {
				sketchVectorMV.push_back(new MVSketch<MV_TYPE, MV_DEPTH, MV_WIDTH>(HH_ESTIMATE_THRESHOLD));
			}
			OmniWindowSketch<MV_TYPE> omMV(sketchVectorMV, data);
			omMV.insertAll();
			omMV.printResults();
		}
		else if (*it == "HP") {
			std::vector<SketchBase<HP_TYPE> *> sketchVectorHP;
			for (int i = 0; i < MEASURE_POINT; ++i) {
				sketchVectorHP.push_back(new HashPipe<HP_TYPE, HP_DEPTH, HP_WIDTH>(HH_ESTIMATE_THRESHOLD));
			}
			OmniWindowSketch<HP_TYPE> omHP(sketchVectorHP, data);
			omHP.insertAll();
			omHP.printResults();
		}
		else if (*it == "SS") {
			std::vector<SketchBase<SS_TYPE> *> sketchVectorSS;
			for (int i = 0; i < MEASURE_POINT; ++i) {
				sketchVectorSS.push_back(new SpreadSketch<SS_TYPE, SS_DEPTH, SS_WIDTH>(SS_B, SS_BB, SS_C, SS_ESTIMATE_THRESHOLD));
			}
			OmniWindowSketch<SS_TYPE> omSS(sketchVectorSS, data);
			omSS.insertAll();
			omSS.printResults();
		}
		else if (*it == "VBF") {
			std::vector<SketchBase<VBF_TYPE> *> sketchVectorVBF;
			for (int i = 0; i < MEASURE_POINT; ++i) {
				sketchVectorVBF.push_back(new VectorBF<VBF_TYPE, VBF_WIDTH>(SS_ESTIMATE_THRESHOLD));
			}
			OmniWindowSketch<VBF_TYPE> omVBF(sketchVectorVBF, data);
			omVBF.insertAll();
			omVBF.printResults();
		}
		else if (*it == "LC") {
			std::vector<SketchBase<LC_TYPE> *> sketchVectorLC;
			for (int i = 0; i < MEASURE_POINT; ++i) {
				sketchVectorLC.push_back(new LinearCounting<LC_TYPE, LC_WIDTH>());
			}
			OmniWindowSketch<LC_TYPE> omLC(sketchVectorLC, data);
			omLC.insertAll();
			omLC.printResults();
		}
		else if (*it == "HLL") {
			std::vector<SketchBase<HLL_TYPE> *> sketchVectorHLL;
			for (int i = 0; i < MEASURE_POINT; ++i) {
				sketchVectorHLL.push_back(new HyperLL<HLL_TYPE, HLL_WIDTH>());
			}
			OmniWindowSketch<HLL_TYPE> omHLL(sketchVectorHLL, data);
			omHLL.insertAll();
			omHLL.printResults();
		}
	}
	
	return 0;
}
