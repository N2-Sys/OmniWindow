#ifndef DISTINCT_HPP
#define DISTINCT_HPP

#include <memory>

#include "Hash.hpp"

template <typename Key>
class Distinct {
public:
  virtual void switchWin() = 0;
  virtual bool process(Key key) = 0;
};

template <typename Key>
class DataPlaneDistinct : public Distinct<Key> {
  int nHash;
  std::vector<std::vector<bool>> a;

  std::vector<bool>::reference get(int i, Key key) {
    return a[i][select_crc(i, &key, sizeof(key)) & (a[i].size() - 1)];
  }

  static constexpr bool isPowerOf2(size_t x) {
    return x && x == (x & -x);
  }

public:
  DataPlaneDistinct(size_t totLen, int nHash_ = 1) :
      nHash(nHash_), a(nHash_, std::vector<bool>(totLen / nHash_, 0)) {
    assert(totLen % nHash == 0);
    assert(isPowerOf2(totLen / nHash));
  }

  void switchWin() override {
    for (int i = 0; i < nHash; i++)
      for (std::vector<bool>::reference x : a[i])
        x = 0;
  }

  bool process(Key key) override {
    bool exist = true;
    for (int i = 0; i < nHash; i++) {
      std::vector<bool>::reference v = get(i, key);
      exist &= v;
      v = 1;
    }
    return !exist;
  }
};

template <typename Key>
class BaselineDistinct : public Distinct<Key> {
  HashSet<Key> a;

public:
  void switchWin() override {
    a.clear();
  }

  bool process(Key key) override {
    return a.insert(key).second;
  }
};

template <typename Key>
class FineGrainedLargeDistinct : public Distinct<Key> {
  size_t curSlide, nSlide;

  std::unique_ptr<Distinct<Key>> underlay;

public:
  FineGrainedLargeDistinct(size_t nSlide_, std::unique_ptr<Distinct<Key>> underlay_) :
      curSlide(0), nSlide(nSlide_), underlay(std::move(underlay_)) {
    return;
  }

  void switchWin() override {
    curSlide = (curSlide + 1) % nSlide;
    if (curSlide == 0)
      underlay->switchWin();
  }

  bool process(Key key) override {
    return underlay->process(key);
  }
};

template <typename Key>
class SlidingBaselineDistinct : public Distinct<Key> {
  size_t nSlide;
  HashMap<Key, size_t> ttl;

public:
  SlidingBaselineDistinct(size_t nSlide_) : nSlide(nSlide_) {}

  void switchWin() override {
    for (auto it = ttl.begin(); it != ttl.end(); ) {
      if (!--it->second)
        it = ttl.erase(it);
      else
        ++it;
    }
  }

  bool process(Key key) override {
    if (auto it = ttl.find(key); it != ttl.end()) {
      it->second = nSlide;
      return false;
    } else {
      ttl.insert({key, nSlide});
      return true;
    }
  }

  size_t getTtlAndProcess(Key key) {
    if (auto it = ttl.find(key); it != ttl.end()) {
      auto r = it->second;
      it->second = nSlide;
      return r;
    } else {
      ttl.insert({key, nSlide});
      return 0;
    }
  }
};

template <typename Key>
class SlidingBaselineDistinctSimple : public Distinct<Key> {
  size_t curSlide, nSlide;
  HashMap<Key, size_t> ttl;

public:
  SlidingBaselineDistinctSimple(size_t nSlide_) : curSlide(0), nSlide(nSlide_) {}

  void switchWin() override {
    for (auto it = ttl.begin(); it != ttl.end(); ) {
      if (!--it->second)
        it = ttl.erase(it);
      else
        ++it;
    }
  }

  bool process(Key key) override {
    if (auto it = ttl.find(key); it != ttl.end()) {
      it->second = nSlide;
      return false;
    } else {
      ttl.insert({key, nSlide});
      return true;
    }
  }
};

#endif
