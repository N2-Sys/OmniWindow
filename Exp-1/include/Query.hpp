#ifndef QUERY_HPP
#define QUERY_HPP

#include <cmath>

#include <memory>
#include <optional>

#include "Trace.h"
#include "Result.hpp"

template <typename Key>
class Query {
public:
  virtual std::optional<HashSet<Key>> switchWin() = 0;
  virtual void process(PktInfo pkt) = 0;
};

template <typename Key>
Result<Key> getResultRaw(const Trace &trace, size_t nEpoch,
    double interval, double dcTime, Query<Key> *query) {
  Result<Key> res;
  size_t curEpoch = 0;
  // double startTime = -INFINITY;

  uint64_t interval_us = (uint64_t)(interval * 1e6);
  uint64_t dcTime_us = (uint64_t)(dcTime * 1e6);
  uint64_t startTime_us = 0;

  for (PktInfo pkt : trace) {
    uint64_t pktTime_us = (uint64_t)(pkt.pkt_ts * 1e6);
    if (startTime_us == 0) {
      startTime_us = pktTime_us;
    }
    while (pktTime_us - startTime_us >= interval_us) {
      if (auto r = query->switchWin())
        res.push_back(std::move(*r));
      startTime_us += interval_us;
      if (++curEpoch == nEpoch)
        goto DONE;
    }
    if (pktTime_us - startTime_us < dcTime_us)
      continue;

    query->process(pkt);
  }
DONE:

  if (curEpoch < nEpoch)
    fprintf(stderr, "[Warning] finished in epoch %lu/%lu\n", curEpoch, nEpoch);

  return res;
}


template <typename Key>
Result<Key> getResult(const Trace &trace, size_t nEpoch,
    double interval, double dcTime, std::unique_ptr<Query<Key>> query) {
  return getResultRaw(trace, nEpoch, interval, dcTime, query.get());
}

#endif
