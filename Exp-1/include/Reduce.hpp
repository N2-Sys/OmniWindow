#ifndef REDUCE_HPP
#define REDUCE_HPP

#include <memory>
#include <optional>

#include "Hash.hpp"

template <typename Key, typename Val = uint32_t>
class Reduce {
public:
  virtual std::optional<HashMap<Key, Val>> switchWin() = 0;
  virtual void process(Key key, Val val) = 0;
};

template <typename Key, typename Val = uint32_t>
class DataPlaneReduce : public Reduce<Key, Val> {
  int nHash;
  std::vector<std::vector<Val>> a;
  std::vector<Key> keys;

  Val &get(int i, Key key) {
    return a[i][select_crc(i, &key, sizeof(key)) & (a[i].size() - 1)];
  }

  Val get(Key key) {
    Val res = std::numeric_limits<Val>::max();
    for (int i = 0; i < nHash; i++)
      res = std::min(res, get(i, key));
    return res;
  }

  static constexpr bool isPowerOf2(size_t x) {
    return x && x == (x & -x);
  }

public:
  DataPlaneReduce(size_t totLen, int nHash_ = 1) :
      nHash(nHash_), a(nHash_, std::vector<Val>(totLen / nHash_, 0)) {
    assert(totLen % nHash == 0);
    assert(isPowerOf2(totLen / nHash));
  }

  std::optional<HashMap<Key, Val>> switchWin() override {
    HashMap<Key, Val> res;
    for (Key key : keys)
      res.insert({key, get(key)});
    for (int i = 0; i < nHash; i++)
      memset(a[i].data(), 0, a[i].size() * sizeof(Val));
    keys.clear();
    return res;
  }

  void process(Key key, Val val) override {
    bool add = false;
    for (int i = 0; i < nHash; i++) {
      Val &v = get(i, key);
      if (v == 0)
        add = true;
      v += val;
    }
    if (add)
      keys.push_back(key);
  }
};

template <typename Key, typename Val = uint32_t>
class BaselineReduce : public Reduce<Key, Val> {
  HashMap<Key, Val> a;

public:
  std::optional<HashMap<Key, Val>> switchWin() override {
    HashMap<Key, Val> res;
    res.swap(a);
    return res;
  }

  void process(Key key, Val val) override {
    a[key] += val;
  }
};

template <typename Key, typename Val = uint32_t>
class FineGrainedReduce : public Reduce<Key, Val> {
  size_t curSlide, nSlide;
  HashMap<Key, Val> mergedData;

  std::unique_ptr<Reduce<Key, Val>> underlay;

public:
  FineGrainedReduce(size_t nSlide_, std::unique_ptr<Reduce<Key, Val>> underlay_) :
      curSlide(0), nSlide(nSlide_), underlay(std::move(underlay_)) {
    return;
  }

  std::optional<HashMap<Key, Val>> switchWin() override {
    if (auto r = underlay->switchWin()) {
      for (auto [key, val] : *r)
        mergedData[key] += val;

      curSlide = (curSlide + 1) % nSlide;
      if (curSlide == 0) {
        HashMap<Key, Val> res;
        res.swap(mergedData);
        return res;
      }
    }
    return std::nullopt;
  }

  void process(Key key, Val val) override {
    underlay->process(key, val);
  }
};

template <typename Key, typename Val = uint32_t>
class SlidingReduce : public Reduce<Key, Val> {
  size_t curSlide, nSlide;
  std::vector<HashMap<Key, Val>> slideData;
  HashMap<Key, Val> mergedData;

  std::unique_ptr<Reduce<Key, Val>> underlay;

public:
  SlidingReduce(size_t nSlide_, std::unique_ptr<Reduce<Key, Val>> underlay_) :
      curSlide(0), nSlide(nSlide_), slideData(nSlide_), underlay(std::move(underlay_)) {
    return;
  }

  std::optional<HashMap<Key, Val>> switchWin() override {
    if (auto r = underlay->switchWin()) {
      for (auto [key, val] : slideData[curSlide])
        if (!(mergedData[key] -= val))
          mergedData.erase(key);
      r->swap(slideData[curSlide]);
      for (auto [key, val] : slideData[curSlide])
        mergedData[key] += val;

      curSlide = (curSlide + 1) % nSlide;
      return mergedData;
    }
    return std::nullopt;
  }

  void process(Key key, Val val) override {
    underlay->process(key, val);
  }

  void processWithPrevTtl(Key key, Val val, size_t prevTtl) {
    if (prevTtl < nSlide) {
      if (prevTtl) {
        slideData[(curSlide + prevTtl) % nSlide][key] -= val;
        mergedData[key] -= val;
      }
      underlay->process(key, val);
    }
  }
};

#endif
