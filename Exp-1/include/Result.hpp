#ifndef RESULT_HPP
#define RESULT_HPP

#include <algorithm>

#include "Hash.hpp"

template <typename Key>
using Result = std::vector<HashSet<Key>>;

template <typename Key>
Result<Key> collapseResult(const Result<Key> &orig, size_t start, size_t width, size_t stride) {
  size_t n = orig.size();
  Result<Key> res;
  for (size_t i = start; i + width <= n; i += stride) {
    HashSet<Key> a;
    for (size_t j = 0; j < width; j++)
      for (Key k : orig[i + j])
        a.insert(k);
    res.push_back(std::move(a));
  }
  return res;
}

template <typename Key>
Result<Key> collapseResultAll(const Result<Key> &orig) {
  return collapseResult(orig, 0, orig.size(), 1);
}

template <typename Key>
struct ResultInfo {
  size_t nEpoch;
  std::vector<double> precision, recall;
  double avgAbnormal;
  double avgPrecision;
  double avgRecall;

  ResultInfo(const Result<Key> &baseline, const Result<Key> &result) :
      nEpoch(baseline.size()), precision(nEpoch), recall(nEpoch) {
    assert(result.size() == nEpoch);

    avgAbnormal = avgPrecision = avgRecall = 0;
    for (size_t i = 0; i != nEpoch; i++) {
      size_t detectedAbnormal = 0, detectedNormal = 0;
      for (Key k : result[i]) {
        if (baseline[i].count(k))
          detectedAbnormal++;
        else
          detectedNormal++;
      }
      avgAbnormal += (double)baseline[i].size();
      avgPrecision += precision[i] = result[i].empty() ? 1.0 : (double)detectedAbnormal / (double)result[i].size();
      avgRecall += recall[i] = baseline[i].empty() ? 1.0 : (double)detectedAbnormal / (double)baseline[i].size();
    }

    avgAbnormal /= (double)nEpoch;
    avgPrecision /= (double)nEpoch;
    avgRecall /= (double)nEpoch;
  }

  void print(const char *prefix = "", FILE *fp = stdout) {
    fprintf(fp, "%s\n  Num Epoch %lu\n"
          "  Baseline Abnormal avg. %f\n"
          "  Precision avg. %f\n  Recall avg. %f\n",
        prefix, nEpoch, avgAbnormal, avgPrecision, avgRecall);
    fflush(fp);
  }
};

#endif
