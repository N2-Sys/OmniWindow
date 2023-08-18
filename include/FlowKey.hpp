#ifndef FLOWKEY_HPP
#define FLOWKEY_HPP

#include <cstdint>
#include <cstring>

#include <vector>

struct FlowKey {
  uint32_t dstIp;
  uint32_t srcIp;
  uint16_t dstPort;
  uint16_t srcPort;
  uint8_t protocol;
} __attribute__((packed));

inline bool operator ==(const FlowKey &a, const FlowKey &b) {
  return memcmp(&a, &b, sizeof(FlowKey)) == 0;
}

inline std::vector<FlowKey> readKeys(const char *path) {
  FILE *fp = fopen(path, "r");
  uint64_t n;

  if (fread(&n, sizeof(n), 1, fp) != 1) {
    fclose(fp);
    throw std::runtime_error("invalid key buf format");
  }

  std::vector<FlowKey> a(n);
  if (fread(a.data(), sizeof(FlowKey), n, fp) != n) {
    fclose(fp);
    throw std::runtime_error("invalid key buf format");
  }

  fclose(fp);
  return a;
}

#endif
