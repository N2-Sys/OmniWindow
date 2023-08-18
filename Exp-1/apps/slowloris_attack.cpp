#include "Query.hpp"
#include "Distinct.hpp"
#include "Reduce.hpp"
#include "utils.hpp"

#include "evaluate.hpp"

using Key = uint32_t;

struct DKey {
  uint32_t dst_ip;
  uint32_t src_ip;
  uint32_t src_port;
} __attribute__((packed));

class Slowloris : public Query<Key> {
  uint64_t threshold1;
  double threshold2;
  std::unique_ptr<Distinct<DKey>> distinct1;
  std::unique_ptr<Reduce<Key, uint64_t>> reduceCon, reduceBytes;

public:
  std::vector<uint64_t> val1s;
  std::vector<double> val2s;

  Slowloris(uint64_t threshold1_, double threshold2_,
        std::unique_ptr<Distinct<DKey>> distinct1_,
        std::unique_ptr<Reduce<Key, uint64_t>> reduce1_,
        std::unique_ptr<Reduce<Key, uint64_t>> reduce2_) :
      threshold1(threshold1_), threshold2(threshold2_),
      distinct1(std::move(distinct1_)),
      reduceCon(std::move(reduce1_)),
      reduceBytes(std::move(reduce2_)) {}

  std::optional<HashSet<Key>> switchWin() override {
    distinct1->switchWin();
    auto rCon = reduceCon->switchWin();
    auto rBytes = reduceBytes->switchWin();
    if (rCon && rBytes) {
      HashSet<Key> res;
      for (auto [key, vBytes] : *rBytes) {
        if (rCon->count(key)) {
          uint64_t vCon = (*rCon)[key];
          double v = (double)vCon / (double)vBytes;
          val1s.push_back(vBytes);
          val2s.push_back(v);
          if (vBytes >= threshold1) {
            if (v >= threshold2)
              res.insert(key);
          }
        }
      }
      return res;
    }
    return std::nullopt;
  }

  void process(PktInfo pkt) override {
    if (pkt.key.proto == 6) {
      DKey dkey{pkt.key.dst_ip, pkt.key.src_ip, pkt.key.src_port};
      Key key = pkt.key.dst_ip;

      if (auto *sDistinct = dynamic_cast<SlidingBaselineDistinct<DKey> *>(distinct1.get())) {
        auto *sReduce = dynamic_cast<SlidingReduce<Key, uint64_t> *>(reduceCon.get());
        assert(sReduce);
        size_t prevTtl = sDistinct->getTtlAndProcess(dkey);
        sReduce->processWithPrevTtl(key, 1, prevTtl);
      } else {
        if (distinct1->process(dkey))
          reduceCon->process(key, 1);
      }

      reduceBytes->process(key, pkt.size);
    }
  }
};

int main(int argc, char **argv) {
  uint32_t threshold1;
  double threshold2;

  if (argc != 4 ||
      sscanf(argv[2], "%u", &threshold1) != 1 ||
      sscanf(argv[3], "%lf", &threshold2) != 1) {
    fprintf(stderr, "%s <trace> <threshold-1> <threshold-2>\n", argv[0]);
    return 1;
  }

  auto qryGen = [threshold1, threshold2](auto disGen, auto redGen) {
    return std::make_unique<Slowloris>(
      threshold1, threshold2,
      disGen(),
      redGen(),
      redGen());
  };

  printf("Threshold: %u %f\n\n", threshold1, threshold2);

  evaluate<Key, DKey, Key, uint64_t>(argv[1], qryGen, 65536 * 32 * 2, 65536 * 2);

  return 0;
}
