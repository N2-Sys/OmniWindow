const bit<16> UDP_PORT_ROCE = 4791;
const bit<16> UDP_PORT_DC   = 33669;

enum bit<8> ib_bth_op_t {
  RC_SEND_ONLY                = 4,
  RC_RDMA_WRITE_ONLY          = 10,
  RC_RDMA_WRITE_ONLY_WITH_IMM = 11,
  RC_FETCH_ADD                = 20
}

header ib_bth_h {
  ib_bth_op_t opcode;
  bit<1> sol_event;
  bit<1> mig_req;
  bit<2> pad_cnt;
  bit<4> hdr_ver;
  bit<16> p_key;
  bit<8> rsrv0;
  bit<24> dst_qp;
  bit<1> ack;
  bit<7> rsrv1;
  bit<24> psn;
}

header ib_reth_h {
  bit<64> vaddr;
  bit<32> rkey;
  bit<32> dma_len;
}

header ib_immdt_h {
  bit<32> val;
}

header ib_icrc_h {
  bit<8> v7_0;
  bit<8> v15_8;
  bit<8> v23_16;
  bit<8> v31_24;
}

#define RWR_ICRC_HDR \
  64w0xffffffffffffffff, \
  hdr.ipv4.version, \
  hdr.ipv4.ihl, \
  8w0xff, /* hdr.ipv4.diffserv */ \
  hdr.ipv4.total_len, \
  hdr.ipv4.identification, \
  hdr.ipv4.flags, \
  hdr.ipv4.frag_offset, \
  8w0xff, /* hdr.ipv4.ttl */ \
  hdr.ipv4.protocol, \
  16w0xffff, /* hdr.ipv4.hdr_checksum */ \
  hdr.ipv4.src_addr, \
  hdr.ipv4.dst_addr, \
 \
  hdr.udp.src_port, \
  hdr.udp.dst_port, \
  hdr.udp.hdr_length, \
  16w0xffff, /* hdr.udp.checksum */ \
 \
  hdr.ib_bth.opcode, \
  hdr.ib_bth.sol_event, \
  hdr.ib_bth.mig_req, \
  hdr.ib_bth.pad_cnt, \
  hdr.ib_bth.hdr_ver, \
  hdr.ib_bth.p_key, \
  8w0xff, /* bit<8> rsrv0 */ \
  hdr.ib_bth.dst_qp, \
  hdr.ib_bth.ack, \
  hdr.ib_bth.rsrv1, \
  hdr.ib_bth.psn, \
 \
  hdr.ib_reth.vaddr, \
  hdr.ib_reth.rkey, \
  hdr.ib_reth.dma_len
