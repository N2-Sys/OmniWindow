#include "CMSketch.h"
#include "SuMax.h"
#include "lossRadar.h"

const char * dataPath = "/data/OmniWindow-trace/equinix-nyc-modified-tuples.dirB.20180419-130100.UTC.anon.bin";

int main() {
    // SuMaxSketch<CM_TYPE, CM_DEPTH, CM_WIDTH> cm1;
    // SuMaxSketch<CM_TYPE, CM_DEPTH, CM_WIDTH> cm2;
    LossRadar<uint32_t, 123362, 4> lr1;
    LossRadar<uint32_t, 123362, 4> lr2;
    DataProfiler *data = new DataProfiler(dataPath, true);
    // std::set<Key_t> loss;

    double start = data->traces[0].begin()->timestamp;
    double end = start + 0.5;

    uint32_t packet_id = 0;

    for (int i = 0; i < MEASURE_POINT; ++i) {
        for (auto j = data->traces[i].begin(); j != data->traces[i].end(); ++j) {
            lr1.insert(CustomizedKey(j->key, ++packet_id));
            if (data->lossFlow.find(j->key) != data->lossFlow.end() && data->lossFlow[j->key] == false)
            {
                data->lossFlow[j->key] = true;
                continue;
            }
            if (j->timestamp + 0.000004 < end)
                lr2.insert(CustomizedKey(j->key, packet_id));
        }
    }

    // Detect
    // for (int i = 0; i < MEASURE_POINT; ++i) {
    //     for (auto j = data->traces[i].begin(); j != data->traces[i].end(); ++j) {
    //         uint32_t ret1 = cm1.query(j->key);
    //         uint32_t ret2 = cm2.query(j->key);

    //         if (ret2 < ret1) {
    //             loss.insert(j->key);
    //         }
    //     }
    // }

    lr1 ^= lr2;
    lr1.dump();

    // Calculate
    uint32_t tp = 0, fp = 0, fn = 0;
    for (auto i = lr1.lost_flows.begin(); i != lr1.lost_flows.end(); ++i) {
        if (data->lossFlow.find(*i) != data->lossFlow.end()) {
            tp++;
        }
        else {
            fp++;
        }
    }
    for (auto i = data->lossFlow.begin(); i != data->lossFlow.end(); ++i) {
        if (lr1.lost_flows.find(i->first) == lr1.lost_flows.end()) {
            fn++;
        }
    }

    std::cout << tp << " " << fp << " " << fn << std::endl;
    std::cout << "Precision: " << (double)tp / (tp + fp) << std::endl;
    std::cout << "Recall: " << (double)tp / (tp + fn) << std::endl;
}