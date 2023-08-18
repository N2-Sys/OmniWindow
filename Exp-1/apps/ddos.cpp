#include "Query.hpp"
#include "Distinct.hpp"
#include "Reduce.hpp"
#include "utils.hpp"

#include "evaluate.hpp"

using Key = uint32_t;

struct DKey {
  uint32_t dst_ip;
  uint32_t src_ip;
} __attribute__((packed));

class DDoS : public Query<Key> {
  uint32_t threshold;
  std::unique_ptr<Distinct<DKey>> distinct;
  std::unique_ptr<Reduce<Key, uint32_t>> reduce;

public:
  std::vector<uint32_t> vals;

  DDoS(uint32_t threshold_, std::unique_ptr<Distinct<DKey>> distinct_,
        std::unique_ptr<Reduce<Key, uint32_t>> reduce_) :
      threshold(threshold_), distinct(std::move(distinct_)), reduce(std::move(reduce_)) {}

  std::optional<HashSet<Key>> switchWin() override {
    distinct->switchWin();
    if (auto r = reduce->switchWin()) {
      HashSet<Key> res;
      for (auto [key, val] : *r) {
        vals.push_back(val);
        if (val >= threshold)
          res.insert(key);
      }
      return res;
    }
    return std::nullopt;
  }

  void process(PktInfo pkt) override {
    DKey dkey{pkt.key.dst_ip, pkt.key.src_ip};
    Key key = pkt.key.dst_ip;

    if (auto *sDistinct = dynamic_cast<SlidingBaselineDistinct<DKey> *>(distinct.get()))
      if (auto *sReduce = dynamic_cast<SlidingReduce<Key, uint32_t> *>(reduce.get())) {
        size_t prevTtl = sDistinct->getTtlAndProcess(dkey);
        sReduce->processWithPrevTtl(key, 1, prevTtl);
        return;
      }

    if (distinct->process(dkey))
      reduce->process(key, 1);
  }
};

int main(int argc, char **argv) {
  uint32_t threshold;

  if (argc != 3 || sscanf(argv[2], "%u", &threshold) != 1) {
    fprintf(stderr, "%s <trace> <threshold>\n", argv[0]);
    return 1;
  }

  auto qryGen = [threshold](auto disGen, auto redGen) {
    return std::make_unique<DDoS>(
      threshold,
      disGen(),
      redGen()
    );
  };

  printf("Threshold: %u\n\n", threshold);

  evaluate<Key, DKey, Key, uint32_t>(argv[1], qryGen);

  return 0;
}
