#include "Query.hpp"
#include "Reduce.hpp"
#include "utils.hpp"

#include "evaluate.hpp"

using Key = uint32_t;

class SSHBrute : public Query<Key> {
  int32_t threshold;
  std::unique_ptr<Reduce<Key, uint32_t>> reduce1, reduce2, reduce3;

public:
  std::vector<int32_t> vals;

  SSHBrute(uint32_t threshold_,
        std::unique_ptr<Reduce<Key, uint32_t>> reduce1_,
        std::unique_ptr<Reduce<Key, uint32_t>> reduce2_,
        std::unique_ptr<Reduce<Key, uint32_t>> reduce3_) :
      threshold(threshold_), reduce1(std::move(reduce1_)),
      reduce2(std::move(reduce2_)), reduce3(std::move(reduce3_)) {}

  std::optional<HashSet<Key>> switchWin() override {
    auto r1 = reduce1->switchWin();
    auto r2 = reduce2->switchWin();
    auto r3 = reduce3->switchWin();
    if (r1 && r2 && r3) {
      HashSet<Key> res;
      for (auto [key, val] : *r1) {
        int32_t v = (int32_t)(val + (*r2)[key] - (*r3)[key]);
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
      if (pkt.tcp_flag == 17)
        reduce2->process(pkt.key.src_ip, 1);
      if (pkt.tcp_flag == 16)
        reduce3->process(pkt.key.dst_ip, 1);
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
    return std::make_unique<SSHBrute>(
      threshold,
      redGen(),
      redGen(),
      redGen());
  };

  printf("Threshold: %d\n\n", threshold);

  evaluate<Key, VoidDKey, Key, uint32_t>(argv[1], qryGen);

  return 0;
}
