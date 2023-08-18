#ifndef HASH_TABLE_HPP
#define HASH_TABLE_HPP

#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_errno.h>

#include <string>
#include <exception>

class HashTableException : std::exception {
  std::string desc;

public:
  HashTableException(std::string desc_) : desc(std::move(desc_)) {}

  const char *what() const noexcept override {
    return this->desc.c_str();
  }
};

class HashTable {
  rte_hash *h;

public:
  HashTable(uint32_t nEntries, uint32_t keyLen) {
    rte_hash_parameters params {
      .entries = nEntries,
      .key_len = keyLen,
    };
    h = rte_hash_create(&params);
    if (!h) {
      fprintf(stderr, "rte_hash_create: %s\n", strerror(rte_errno));
      throw HashTableException("rte_hash_create");
    }
  }

  HashTable(const HashTable &) = delete;

  ~HashTable() {
    rte_hash_free(h);
  }

  int32_t addKey(const void *key) {
    int32_t rc = rte_hash_add_key(h, key);
    if (rc < 0)
      throw HashTableException("rte_hash_add_key");
    return rc;
  }

  int32_t lookup(const void *key) {
    int32_t rc = rte_hash_lookup(h, key);
    if (rc == -ENOENT)
      return -1;
    if (rc < 0)
      throw HashTableException("rte_hash_lookup");
    return rc;
  }

  void delKey(const void *key) {
    int32_t pos = rte_hash_del_key(h, key);
    if (pos < 0)
      throw HashTableException("rte_hash_del_key");
    // int rc = rte_hash_free_key_with_position(h, pos);
    // if (rc < 0)
    //   throw HashTableException("rte_hash_free_key_with_position");
  }

  int32_t maxKey() {
    return rte_hash_max_key_id(h);
  }

  void clear() {
    rte_hash_reset(h);
  }

  class Iterator {
    HashTable *h;
    uint32_t it;

    Iterator(HashTable *h_) : h(h_), it(0) {}

    friend class HashTable;

  public:
    struct Item {
      const void *key;
      void *data;
      int32_t pos;
    } v;

    Item &operator *() {
      return v;
    }

    Iterator &operator++() {
      v.pos = rte_hash_iterate(h->h, &v.key, &v.data, &it);
      if (v.pos < 0 && v.pos != -ENOENT)
        throw HashTableException("rte_hash_iterate");
      return *this;
    }

    bool operator != (const Iterator &x) const {
      return v.pos != x.v.pos;
    }
  };

  Iterator begin() {
    Iterator r(this);
    return ++r;
  }

  Iterator end() {
    Iterator r(this);
    r.v.pos = -ENOENT;
    return r;
  }
};

#endif
