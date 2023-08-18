#ifndef HASH_HPP
#define HASH_HPP

#include <cassert>
#include <cstring>

#include <vector>
#include <unordered_map>
#include <unordered_set>

uint32_t crc32(const void *data, uint16_t len);
uint32_t select_crc(int hashid, const void *data, uint16_t len);

template <typename Key>
class Hash {
public:
  size_t operator ()(Key k) const {
    static_assert(sizeof(Key) <= UINT16_MAX);
    return crc32(&k, sizeof(Key));
  }
};

template <typename Key>
class MemEqTo {
public:
  bool operator ()(Key x, Key y) const {
    return memcmp(&x, &y, sizeof(Key)) == 0;
  }
};

template <typename Key>
using HashSet = std::unordered_set<Key, Hash<Key>, MemEqTo<Key>>;

template <typename Key, typename Val>
using HashMap = std::unordered_map<Key, Val, Hash<Key>, MemEqTo<Key>>;

#endif
