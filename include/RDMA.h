#include <infiniband/verbs.h>

#include <string>
#include <exception>

class RDMAException : public std::exception {
  std::string desc;

public:
  RDMAException(std::string desc_) : desc(std::move(desc_)) {}

  const char *what() const noexcept override {
    return this->desc.c_str();
  }
};

class RDMA {
private:
  ibv_context *ctx;
  ibv_pd *pd;
  ibv_mr *mr;
  ibv_cq *cq;
  ibv_qp *qp;

  void destroy();

public:
  void *mem;
  size_t memLen;

  RDMA(const char *devName, const char *ip, void *mem, size_t memLen);
  RDMA(const RDMA &) = delete;
  ~RDMA();

  void postRecv(uint64_t wrId, void *buf, uint32_t len);
  void pollCQ(ibv_wc *wc);

  uint32_t getQPN() const;
  uint32_t getRKey() const;
};
