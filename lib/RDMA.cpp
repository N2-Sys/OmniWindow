#include "RDMA.h"

#include <cstdlib>

#include <malloc.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/mman.h>
#include <arpa/inet.h>

// constexpr uintptr_t MMAP_ADDR = 0x600000000000UL;

static int cmpfunc(const void *a, const void *b) {
  return (*(int *)a - *(int *)b);
}

static bool is_ipv4_gid(const char *devName, int devPort, int gid_index) {
  char file_name[384] = {0};
  static const char ipv4_gid_prefix[] = "0000:0000:0000:0000:0000:ffff:";
  FILE *fp = NULL;
  ssize_t read;
  char *line = NULL;
  size_t len = 0;
  snprintf(file_name, sizeof(file_name),
           "/sys/class/infiniband/%s/ports/%d/gids/%d", devName,
           devPort, gid_index);
  fp = fopen(file_name, "r");
  if (!fp) {
    return false;
  }
  read = getline(&line, &len, fp);
  fclose(fp);
  if (!read) {
    return false;
  }
  return strncmp(line, ipv4_gid_prefix, strlen(ipv4_gid_prefix)) == 0;
}


static int get_rocev2_gid_index(const char *devName, int devPort) {
  static constexpr int MAX_GID_COUNT = 128;
  int gid_index_list[MAX_GID_COUNT];
  int gid_count = 0;

  char dir_name[128] = {0};
  snprintf(dir_name, sizeof(dir_name),
           "/sys/class/infiniband/%s/ports/%d/gid_attrs/types",
           devName, devPort);

  do {
    DIR *dir = opendir(dir_name);
    if (!dir) {
      fprintf(stderr, "Fail to open folder %s\n", dir_name);
      throw RDMAException("opendir");
    }

    struct dirent *dp = NULL;
    char file_name[384] = {0};
    FILE *fp = NULL;
    ssize_t read;
    char line[128];
    size_t len = 0;
    int gid_index;

    while ((dp = readdir(dir)) && gid_count < MAX_GID_COUNT) {
      gid_index = atoi(dp->d_name);
      if (gid_index < 2) {
        continue;
      }

      snprintf(file_name, sizeof(file_name), "%s/%s", dir_name, dp->d_name);
      fp = fopen(file_name, "r");
      if (!fp) {
        // perror("fopen");
        continue;
      }

      errno = 0;
      read = fread(line, 1, sizeof(line), fp);
      if (read <= 0) {
        // perror("fread");
        fclose(fp);
        continue;
      }
      fclose(fp);

      // fprintf(stderr, "%d: %s\n", gid_index, line);

      if (strncmp(line, "RoCE v2", strlen("RoCE v2")) != 0) {
        continue;
      }

      if (!is_ipv4_gid(devName, devPort, gid_index)) {
        continue;
      }

      gid_index_list[gid_count++] = gid_index;
    }

    closedir(dir);

    qsort(gid_index_list, gid_count, sizeof(int), cmpfunc);
  } while (gid_count == 0);

  return gid_index_list[0];
}

RDMA::RDMA(const char *devName, const char *ip, void *mem_, size_t memLen_) : mem(mem_), memLen(memLen_) {
  ctx = nullptr;
  pd = nullptr;
  mr = nullptr;
  cq = nullptr;
  qp = nullptr;

  ibv_device **devs = nullptr;

  try {
    int rc;

    int nDevs;
    devs = ibv_get_device_list(&nDevs);
    if (!devs)
      throw RDMAException("ibv_get_device_list");

    int devId = -1;
    for (int i = 0; i < nDevs; i++)
      if (strcmp(devName, ibv_get_device_name(devs[i])) == 0) {
        devId = i;
        break;
      }
    if (devId == -1)
      throw RDMAException("device not found");

    ctx = ibv_open_device(devs[devId]);
    if (!ctx)
      throw RDMAException("ibv_open_device");

    pd = ibv_alloc_pd(ctx);
    if (!pd)
      throw RDMAException("ibv_alloc_pd");

    // TODO: Allocate Mem
    // mem = (char *)memalign(sysconf(_SC_PAGESIZE), memLen);

    // Fix the address for testing
    // mem = (char *)mmap((void *)MMAP_ADDR, memLen, PROT_READ | PROT_WRITE,
    //   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    // if ((uintptr_t)mem != MMAP_ADDR) {
    //   throw RDMAException(std::string("mmap: ") + strerror(errno));
    // }

    unsigned accessFlags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                        IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC;

    mr = ibv_reg_mr(pd, mem, memLen, accessFlags);
    if (!mr)
      throw RDMAException("ibv_reg_mr");

    // TODO: CQE?
    cq = ibv_create_cq(ctx, 65536, nullptr, 0, 0);
    if (!cq)
      throw RDMAException("ibv_create_cq");

    ibv_qp_init_attr qpInitAttr {
      .send_cq = cq,
      .recv_cq = cq,
      .cap {
        .max_send_wr = 1024,
        .max_recv_wr = 8192,
        .max_send_sge = 1,
        .max_recv_sge = 1,
        .max_inline_data = 512
      },
      .qp_type = IBV_QPT_RC
    };
    qp = ibv_create_qp(pd, &qpInitAttr);
    if (!qp)
      throw RDMAException("ibv_create_qp");

    ibv_qp_attr qpAttr {
      .qp_state = IBV_QPS_INIT,
      .qp_access_flags = accessFlags,
      .pkey_index = 0,
      .port_num = 1
    };
    rc = ibv_modify_qp(qp, &qpAttr,
                      IBV_QP_STATE | IBV_QP_ACCESS_FLAGS |
                          IBV_QP_PKEY_INDEX | IBV_QP_PORT);
    if (rc != 0)
      throw RDMAException("ibv_modify_qp to INIT");

    char buf[128];
    sprintf(buf, "::ffff:%s", ip);
    // TODO: correct GID
    union ibv_gid dgid;
    if (inet_pton(AF_INET6, buf, &dgid) != 1)
      throw RDMAException("inet_pton");

    qpAttr = {
      .qp_state = IBV_QPS_RTR,
      .path_mtu = IBV_MTU_512,
      .rq_psn = 0,
      .dest_qp_num = 0,
      .ah_attr {
        .grh {
          .dgid = dgid,
          .sgid_index = (uint8_t)get_rocev2_gid_index(devName, 1),
          .hop_limit = 1
        },
        .dlid = 0,
        .is_global = 1,
        .port_num = 1,
      },
      .max_dest_rd_atomic = 16,
      .min_rnr_timer = 12,
    };
    rc = ibv_modify_qp(qp, &qpAttr,
        IBV_QP_STATE | IBV_QP_PATH_MTU | IBV_QP_RQ_PSN | IBV_QP_DEST_QPN |
            IBV_QP_AV | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER);
    if (rc != 0)
      throw RDMAException("ibv_modify_qp to RTR");

  } catch (std::exception &e) {
    ibv_free_device_list(devs);
    destroy();
    throw;
  }

  ibv_free_device_list(devs);
}

RDMA::~RDMA() {
  destroy();
}

void RDMA::destroy() {
  if (qp) {
    ibv_destroy_qp(qp);
    qp = nullptr;
  }
  if (cq) {
    ibv_destroy_cq(cq);
    cq = nullptr;
  }
  if (mr) {
    ibv_dereg_mr(mr);
    mr = nullptr;
  }
  if (pd) {
    ibv_dealloc_pd(pd);
    pd = nullptr;
  }
  if (ctx) {
    ibv_close_device(ctx);
    ctx = nullptr;
  }
  // if (mem) {
  //   munmap(mem, memLen);
  //   mem = nullptr;
  // }
}

void RDMA::postRecv(uint64_t wrId, void *buf, uint32_t len) {
  struct ibv_sge sgList {
    .addr = (uintptr_t)buf,
    .length = len,
    .lkey = mr->lkey
  };
  ibv_recv_wr wr {
    .wr_id = wrId,
    .sg_list = buf ? &sgList : 0,
    .num_sge = buf ? 1 : 0
  };
  ibv_recv_wr *bad_wr;
  int rc = ibv_post_recv(qp, &wr, &bad_wr);
  if (rc != 0)
    throw RDMAException("ibv_post_recv");
}

void RDMA::pollCQ(ibv_wc *wc) {
  int rc;
  do {
    rc = ibv_poll_cq(cq, 1, wc);
  } while (rc == 0);

  if (rc < 0 || wc->status != IBV_WC_SUCCESS) {
    if (rc < 0)
      fprintf(stderr, "ibv_poll_cq: fail (%d)\n", rc);
    else {
      fprintf(stderr, "ibv_poll_cq: wc status is '%s'\n", ibv_wc_status_str(wc->status));
      fprintf(stderr, "wc opcode: %d\n", wc->opcode);
    }

    throw RDMAException("ibv_poll_cq");
  }
}

uint32_t RDMA::getQPN() const {
  return qp->qp_num;
}

uint32_t RDMA::getRKey() const {
  return mr->rkey;
}
