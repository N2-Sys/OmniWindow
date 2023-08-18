#include "Trace.h"

#include <cstdio>
#include <cstdlib>

#include <stdexcept>

Trace readTrace(const char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp)
    throw std::runtime_error("fopen");

  int rc = fseek(fp, 0, SEEK_END);
  if (rc < 0)
    throw std::runtime_error("fseek");
  long fsize = ftell(fp);
  if (fsize < 0)
    throw std::runtime_error("ftell");
  rewind(fp);

  if (fsize % sizeof(PktInfo) != 0)
    throw std::runtime_error("incorrect trace file length");
  size_t n = fsize / sizeof(PktInfo);

  Trace res(n);
  if (fread(res.data(), sizeof(PktInfo), n, fp) != n)
    throw std::runtime_error("fread");

  fclose(fp);
  return res;
}

void writeTrace(const char *path, const Trace &trace) {
  FILE *fp = fopen(path, "w");
  if (!fp)
    throw std::runtime_error("fopen");

  if (fwrite(trace.data(), sizeof(PktInfo), trace.size(), fp) != trace.size())
    throw std::runtime_error("fwrite");

  fclose(fp);
}
