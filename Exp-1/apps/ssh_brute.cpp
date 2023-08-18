#include "Query.hpp"
#include "Distinct.hpp"
#include "Reduce.hpp"
#include "utils.hpp"

#include "evaluate.hpp"

struct DKey {
  uint32_t dst_ip;
  uint32_t src_ip;
  int32_t size;
} __attribute__((packed));

struct RKey {
  uint32_t dst_ip;
  int32_t size;
} __attribute__((packed));

using Key = uint32_t;

class SSHBrute : public Query<Key> {
  uint32_t threshold;
  std::unique_ptr<Distinct<DKey>> distinct;
  std::unique_ptr<Reduce<RKey, uint32_t>> reduce;

public:
  std::vector<uint32_t> vals;

  SSHBrute(uint32_t threshold_, std::unique_ptr<Distinct<DKey>> distinct_,
        std::unique_ptr<Reduce<RKey, uint32_t>> reduce_) :
      threshold(threshold_), distinct(std::move(distinct_)), reduce(std::move(reduce_)) {}

  std::optional<HashSet<Key>> switchWin() override {
    distinct->switchWin();
    if (auto r = reduce->switchWin()) {
      HashSet<Key> res;
      for (auto [key, val] : *r) {
        vals.push_back(val);
        if (val >= threshold)
          res.insert(key.dst_ip);
      }
      return res;
    }
    return std::nullopt;
  }

  void process(PktInfo pkt) override {
    if (pkt.key.proto == 6 && pkt.key.dst_port == 22) {
      DKey dkey{pkt.key.dst_ip, pkt.key.src_ip, pkt.size};
      RKey rkey{pkt.key.dst_ip, pkt.size};

      if (auto *sDistinct = dynamic_cast<SlidingBaselineDistinct<DKey> *>(distinct.get()))
        if (auto *sReduce = dynamic_cast<SlidingReduce<RKey, uint32_t> *>(reduce.get())) {
          size_t prevTtl = sDistinct->getTtlAndProcess(dkey);
          sReduce->processWithPrevTtl(rkey, 1, prevTtl);
          return;
        }

      if (distinct->process(dkey))
        reduce->process(rkey, 1);
    }
  }
};

int main(int argc, char **argv) {
  uint32_t threshold;

  if (argc != 3 || sscanf(argv[2], "%u", &threshold) != 1) {
    fprintf(stderr, "%s <trace> <threshold>\n", argv[0]);
    return 1;
  }

  auto qryGen = [threshold](auto disGen, auto redGen) {
    return std::make_unique<SSHBrute>(
      threshold,
      disGen(),
      redGen());
  };

  printf("Threshold: %u\n\n", threshold);

  evaluate<Key, DKey, RKey, uint32_t>(argv[1], qryGen);

  return 0;
}
