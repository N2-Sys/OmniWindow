#include "Query.hpp"
#include "Reduce.hpp"
#include "utils.hpp"

#include "evaluate.hpp"

using Key = uint32_t;

class CompletedFlow : public Query<Key> {
  int32_t threshold;
  std::unique_ptr<Reduce<Key, uint32_t>> reduce1, reduce2;

public:
  std::vector<int32_t> vals;

  CompletedFlow(uint32_t threshold_,
        std::unique_ptr<Reduce<Key, uint32_t>> reduce1_,
        std::unique_ptr<Reduce<Key, uint32_t>> reduce2_) :
      threshold(threshold_), reduce1(std::move(reduce1_)),
      reduce2(std::move(reduce2_)) {}

  std::optional<HashSet<Key>> switchWin() override {
    auto r1 = reduce1->switchWin();
    auto r2 = reduce2->switchWin();
    if (r1 && r2) {
      HashSet<Key> res;
      for (auto [key, v1] : *r1) {
        int32_t v2 = r2->count(key) ? (*r2)[key] : 0;
        int32_t v = (int32_t)v1 - v2;
        vals.push_back(v);
        if (v >= threshold)
          res.insert(key);
      }
      return res;
    }
    return std::nullopt;
  }

  void process(PktInfo pkt) override {
    if (pkt.key.proto == 6) {
      if (pkt.tcp_flag == 2)
        reduce1->process(pkt.key.dst_ip, 1);
      if (pkt.tcp_flag & 1)
        reduce2->process(pkt.key.dst_ip, 1);
    }
  }
};

int main(int argc, char **argv) {
  int32_t threshold;

  if (argc != 3 || sscanf(argv[2], "%d", &threshold) != 1) {
    fprintf(stderr, "%s <trace> <threshold>\n", argv[0]);
    return 1;
  }

  auto qryGen = [threshold]([[maybe_unused]] auto disGen, auto redGen) {
    return std::make_unique<CompletedFlow>(
      threshold,
      redGen(),
      redGen());
  };

  printf("Threshold: %d\n\n", threshold);

  evaluate<Key, VoidDKey, Key, uint32_t>(argv[1], qryGen);

  return 0;
}
