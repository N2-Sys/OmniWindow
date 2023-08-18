#include <vector>
#include <algorithm>

template <typename Val>
Val getQuantile(std::vector<Val> a, double p) {
  std::sort(a.begin(), a.end());
  return a.at(static_cast<size_t>((double)a.size() * p));
}
