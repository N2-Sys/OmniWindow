#include "Query.hpp"
#include "Reduce.hpp"
#include "utils.hpp"

#include "evaluate.hpp"

using Key = uint32_t;

class NewTcpConnections : public Query<Key> {
  uint32_t threshold;
  std::unique_ptr<Reduce<Key, uint32_t>> reduce;

public:
  std::vector<uint32_t> vals;

  NewTcpConnections(uint32_t threshold_, std::unique_ptr<Reduce<Key, uint32_t>> reduce_) :
      threshold(threshold_), reduce(std::move(reduce_)) {}

  std::optional<HashSet<Key>> switchWin() override {
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
    if (pkt.key.proto == 6 && pkt.tcp_flag == 2)
      reduce->process(pkt.key.dst_ip, 1);
  }
};

int main(int argc, char **argv) {
  uint32_t threshold;

  if (argc != 3 || sscanf(argv[2], "%u", &threshold) != 1) {
    fprintf(stderr, "%s <trace> <threshold>\n", argv[0]);
    return 1;
  }

  auto qryGen = [threshold]([[maybe_unused]] auto disGen, auto redGen) {
    return std::make_unique<NewTcpConnections>(
      threshold,
      redGen()
    );
  };

  printf("Threshold: %u\n\n", threshold);

  evaluate<Key, VoidDKey, Key, uint32_t>(argv[1], qryGen);

  return 0;
}
