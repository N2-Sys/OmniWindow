#ifndef COUNT_MIN_HPP
#define COUNT_MIN_HPP

#include <arpa/inet.h>
#include <crcutil/generic_crc.h>

inline crcutil::GenericCrc<uint16_t, uint16_t, uint16_t, 4>
  crc0{0xA001, 16, false},
  crc1{0xA6BC, 16, false},
  crc2{0xEDD1, 16, false},
  crc3{0x91A0, 16, false};

template <typename Key, int N, int A_CM_NUM, int A_MAX_CM_NUM>
class CountMin {
public:
  static_assert(A_CM_NUM <= A_MAX_CM_NUM);
  static_assert(A_CM_NUM <= 4);

  alignas(4096)
  uint16_t data[N][A_MAX_CM_NUM];

  uint16_t query(const Key &key) {
    uint16_t v = ntohs(data[crc0.CrcDefault(&key, sizeof(Key), 0)][0]);
    if constexpr (A_CM_NUM > 1)
      v = std::min(v, ntohs(data[crc1.CrcDefault(&key, sizeof(Key), 0)][1]));
    if constexpr (A_CM_NUM > 2)
      v = std::min(v, ntohs(data[crc2.CrcDefault(&key, sizeof(Key), 0)][2]));
    if constexpr (A_CM_NUM > 3)
      v = std::min(v, ntohs(data[crc3.CrcDefault(&key, sizeof(Key), 0)][3]));
    return v;
  }

  static inline void inc(uint16_t &x) {
    x = htons(ntohs(x) + 1);
  }

  void inc(const Key &key) {
    inc(data[crc0.CrcDefault(&key, sizeof(Key), 0)][0]);
    if constexpr (A_CM_NUM > 1)
      inc(data[crc1.CrcDefault(&key, sizeof(Key), 0)][1]);
    if constexpr (A_CM_NUM > 2)
      inc(data[crc2.CrcDefault(&key, sizeof(Key), 0)][2]);
    if constexpr (A_CM_NUM > 3)
      inc(data[crc3.CrcDefault(&key, sizeof(Key), 0)][3]);
  }
};

#endif
