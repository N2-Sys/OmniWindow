#include <core.p4>
#include <tna.p4>

#include "../../p4-include/headers.p4"
#include "../../p4-include/util.p4"

#include "../../p4-include/data-col.p4"
#include "../../p4-include/count-min.p4"
#include "../../p4-include/bloom-filter.p4"

#if !TUMBLING && !SLIDING
#warning "unspecified window type, using tumbling"
#define TUMBLING
#endif

// #define SLIDING
// #define PROC_OVERLAP
#define EPOCH_DATA_SIZE (1 << 19)

#define NDATA 65536
#define NKEYS 16384

#define MSG_LEN 8
#define MSG_BUF_SIZE (1 << 24)
#define MSG_BATCH_SIZE (1 << 14)

#define RWR_LEN 4

#define IP_HDR_LEN 20
#define UDP_HDR_LEN 8
#define RWR_HDR_LEN (12 + 16)
#define IMMDT_LEN 4
#define ICRC_LEN 4

const bit<3>     MIRROR_TYPE_EG       = 2;
const MirrorId_t MIRROR_SESSION_EG_DC = 2;


// op[7:7] -- start_reset
// op[6:6] -- has_vaddr
// op[4:4] -- push
// op[3:2] -- normal(00), report_key(01), col(10), reset(11)
// op[1:1] -- has_key
// op[0:0] -- is_start
header eg_op_h {
  bit<8> op;
}

// op --- DONE(0), KEY(1), KEY-VAL(2)
header dc_resp_h {
  bit<8> op;
  bit<8> padding;
}

header pad16_h {
  bit<16> val;
}

header key_h {
  bit<32> dst_ip;
}

header vaddr_h {
  bit<64> vaddr;
}

header val_h {
  bit<8> v7_0;
  bit<8> v15_8;
}

struct my_hdr_t {
  eg_op_h    eg_op;

  ethernet_h ethernet;
  ipv4_h     ipv4;
  udp_h      udp;
  tcp_h      tcp;

  ib_bth_h   ib_bth;
  ib_reth_h  ib_reth;
  ib_immdt_h ib_immdt;

  dc_resp_h  dc_resp;
  val_h      val;
  pad16_h    dc_wr_padding;
  key_h      key;
  vaddr_h    vaddr;

  ib_icrc_h  ib_icrc;
}

// Resp Message: op:8 ++ padding:8 ++ val:16 ++ key:32
// Write: val:16 ++ padding:16

// Eg Message: op (++ key) ++ pkt

header eg_mirror_h {
  bit<8>  eg_op;
  bit<32> key_dst_ip;
}

struct ig_md_t {
}

struct eg_md_t {
  bool        ipv4_upd;

  key_h       key;

  MirrorId_t  mirror_session;
  eg_mirror_h mirror_hdr;
}


parser SwitchIngressParser(
    packet_in pkt,
    out my_hdr_t hdr,
    out ig_md_t mg_md,
    out ingress_intrinsic_metadata_t ig_intr_md
  ) {

  TofinoIngressParser() tofino_parser;

  state start {
    tofino_parser.apply(pkt, ig_intr_md);
    transition parse_ethernet;
  }

  state parse_ethernet {
    pkt.extract(hdr.ethernet);
    transition select (hdr.ethernet.ether_type) {
      ETHERTYPE_IPV4: parse_ipv4;
      default:        accept;
    }
  }

  state parse_ipv4 {
    pkt.extract(hdr.ipv4);
    transition select (hdr.ipv4.ihl, hdr.ipv4.protocol) {
      (5, IP_PROTOCOLS_UDP): parse_udp;
      default:               accept;
    }
  }

  state parse_udp {
    pkt.extract(hdr.udp);
    transition select (hdr.udp.dst_port, hdr.udp.src_port) {
      (UDP_PORT_DC, UDP_PORT_DC): parse_eg_op;
      default:                    accept;
    }
  }

  state parse_eg_op {
    pkt.extract(hdr.eg_op);
    transition accept;
  }
}


control SwitchIngress(
    inout my_hdr_t hdr,
    inout ig_md_t ig_md,
    in ingress_intrinsic_metadata_t ig_intr_md,
    in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
    inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {

  action send(PortId_t egress_port) {
    ig_tm_md.ucast_egress_port = egress_port;
  }

  action drop() {
    ig_dprsr_md.drop_ctl = 0x1;
  }

  table tab_forward {
    key = {
      ig_intr_md.ingress_port : exact;
    }

    actions = {
      send;
      @defaultonly drop;
    }

    const default_action = drop();
    size = 512;
  }

  apply {
    if (hdr.eg_op.isValid()) {
      ig_tm_md.ucast_egress_port = ig_intr_md.ingress_port;

      hdr.ethernet.setInvalid();
      hdr.ipv4.setInvalid();
      hdr.udp.setInvalid();

    } else {
      tab_forward.apply();

      hdr.eg_op.setValid();
      hdr.eg_op.op = 8w0b000000000;
    }
  }
}


control SwitchIngressDeparser(
    packet_out pkt,
    inout my_hdr_t hdr,
    in ig_md_t ig_md,
    in ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md) {

  apply {
    pkt.emit(hdr);
  }
}


parser SwitchEgressParser(
    packet_in pkt,
    out my_hdr_t hdr,
    out eg_md_t eg_md,
    out egress_intrinsic_metadata_t eg_intr_md) {

  TofinoEgressParser() tofino_parser;

  state start {
    tofino_parser.apply(pkt, eg_intr_md);

    eg_md.ipv4_upd = false;
    eg_md.mirror_session = 0;

    transition parse_eg_op;
  }

  state parse_eg_op {
    pkt.extract(hdr.eg_op);
    transition select (hdr.eg_op.op[3:2], hdr.eg_op.op[6:6], hdr.eg_op.op[1:1]) {
      (0, _, _): parse_ethernet;
      (_, 0, 1): parse_key;
      (_, 1, 1): parse_key_vaddr;
      default: accept;
    }
  }

  state parse_key {
    pkt.extract(hdr.key);

    eg_md.key = hdr.key;
    eg_md.key.setValid();

    transition accept;
  }

  state parse_key_vaddr {
    pkt.extract(hdr.key);
    pkt.extract(hdr.vaddr);

    eg_md.key = hdr.key;
    eg_md.key.setValid();

    transition accept;
  }

  state parse_ethernet {
    pkt.extract(hdr.ethernet);
    transition select (hdr.ethernet.ether_type) {
      ETHERTYPE_IPV4: parse_ipv4;
      default:        accept;
    }
  }

  state parse_ipv4 {
    pkt.extract(hdr.ipv4);

    eg_md.key.setValid();
    eg_md.key.dst_ip = hdr.ipv4.dst_addr;

    transition select (hdr.ipv4.ihl, hdr.ipv4.protocol) {
      (5, IP_PROTOCOLS_UDP): parse_udp;
      (5, IP_PROTOCOLS_TCP): parse_tcp;
      default:               accept;
    }
  }

  state parse_udp {
    pkt.extract(hdr.udp);
    transition select (hdr.udp.dst_port, hdr.udp.src_port) {
      (UDP_PORT_ROCE, UDP_PORT_ROCE): parse_roce;
      default:                        accept;
    }
  }

  state parse_tcp {
    pkt.extract(hdr.tcp);
    transition accept;
  }

  state parse_roce {
    // to fix compiler PHV allocation only
    // should never be reached
    pkt.extract(hdr.ib_bth);
    pkt.extract(hdr.ib_reth);
    pkt.extract(hdr.ib_immdt);
    pkt.extract(hdr.dc_resp);
    pkt.extract(hdr.val);
    pkt.extract(hdr.dc_wr_padding);
    pkt.extract(hdr.key);
    pkt.extract(hdr.vaddr);
    pkt.extract(hdr.ib_icrc);
    transition accept;
  }
}


control SwitchEgress(
    inout my_hdr_t hdr,
    inout eg_md_t eg_md,
    in egress_intrinsic_metadata_t                 eg_intr_md,
    in egress_intrinsic_metadata_from_parser_t     eg_prsr_md,
    inout egress_intrinsic_metadata_for_deparser_t    eg_dprsr_md,
    inout egress_intrinsic_metadata_for_output_port_t eg_oport_md) {

  Register<bit<8>, bit<0>>(1, 0) reg_epoch;
  bit<8> epoch;
  RegisterAction<bit<8>, bit<0>, bit<8>>(reg_epoch) switch_epoch = {
    void apply(inout bit<8> val, out bit<8> res) {
      res = val;
      val = val + 1;
    }
  };
  RegisterAction<bit<8>, bit<0>, bit<8>>(reg_epoch) get_epoch = {
    void apply(inout bit<8> val, out bit<8> res) {
      res = val;
    }
  };
  RegisterAction<bit<8>, bit<0>, bit<8>>(reg_epoch) get_col_epoch = {
    void apply(inout bit<8> val, out bit<8> res) {
      res = val - 1;
    }
  };

  #define RSTIDX_MID (NDATA * 3)

  // NDATA (reset) -> (NDATA * 3) (start 0) -> 0 (start 1)
  Register<bit<32>, bit<0>>(1, 0) reg_reset_idx;
  RegisterAction<bit<32>, bit<0>, bit<17>>(reg_reset_idx) start_reset_idx = {
    void apply(inout bit<32> val, out bit<17> res) {
      if (val == NDATA) {
        res = NDATA;
        val = RSTIDX_MID;
      } else {
        res = 0;
        val = 1;
      }
    }
  };
  RegisterAction<bit<32>, bit<0>, bit<17>>(reg_reset_idx) get_next_reset_idx = {
    void apply(inout bit<32> val, out bit<17> res) {
      res = (bit<17>)val;
      if (val != NDATA && val != RSTIDX_MID) {
        val = val + 1;
      }
    }
  };

  BLOOM_FILTER_16_DUP(bf_x, 0x3D65)
  BLOOM_FILTER_16_DUP(bf_y, 0x8BB7)

  Register<bit<32>, bit<1>>(2, 0) reg_key_idx;
  RegisterAction<bit<32>, bit<1>, bit<32>>(reg_key_idx) get_next_store_key_idx = {
    void apply(inout bit<32> val, out bit<32> res) {
      res = val;
      if (val != NKEYS) {
        val = val + 1;
      }
    }
  };
  RegisterAction<bit<32>, bit<1>, bit<32>>(reg_key_idx) get_next_col_key_idx = {
    void apply(inout bit<32> val, out bit<32> res) {
      if (val != 0) {
        res = val - 1;
        val = val - 1;
      } else {
        res = NKEYS;
      }
    }
  };

  Register<bit<32>, bit<17>>(1 << 17, 0) reg_dst_ip;
  RegisterAction<bit<32>, bit<17>, bit<32>>(reg_dst_ip) store_dst_ip = {
    void apply(inout bit<32> val) {
      val = eg_md.key.dst_ip;
    }
  };
  RegisterAction<bit<32>, bit<17>, bit<32>>(reg_dst_ip) get_dst_ip = {
    void apply(inout bit<32> val, out bit<32> res) {
      res = val;
    }
  };

  COUNT_MIN_16_DUP(cm_x, 0x8005)
  COUNT_MIN_16_DUP(cm_y, 0x0589)

  Register<bit<32>, bit<0>>(1, 0) reg_psn;
  RegisterAction<bit<32>, bit<0>, bit<24>>(reg_psn) get_next_psn = {
    void apply(inout bit<32> val, out bit<24> res) {
      res = (bit<24>)val;
      val = val + 1;
    }
  };

  Register<bit<32>, bit<0>>(1, 0) reg_msg_addr;
  RegisterAction<bit<32>, bit<0>, bit<64>>(reg_msg_addr) get_next_msg_addr = {
    void apply(inout bit<32> val, out bit<64> res) {
      res = (bit<64>)val;
      if (val == MSG_BUF_SIZE - MSG_LEN) {
        val = 0;
      } else {
        val = val + MSG_LEN;
      }
    }
  };

  action fill_rdma(mac_addr_t src_mac, ipv4_addr_t src_ip,
      mac_addr_t dst_mac, ipv4_addr_t dst_ip,
      bit<24> dst_qp, bit<32> rkey, bit<64> msg_vaddr_base) {
    eg_md.ipv4_upd = true;
    hdr.ethernet = {
        dst_mac,
        src_mac,
        ETHERTYPE_IPV4
    };
    hdr.ethernet.setValid();
    hdr.ipv4 = {
        4,
        5,
        0,
        0, // total_len
        0,
        0x2,
        0,
        64,
        IP_PROTOCOLS_UDP,
        0,
        src_ip,
        dst_ip
    };
    hdr.ipv4.setValid();
    hdr.udp = {
      UDP_PORT_ROCE,
      UDP_PORT_ROCE,
      0, // hdr_length
      0
    };
    hdr.udp.setValid();
    hdr.ib_bth = {
      (ib_bth_op_t)0, // opcode
      0,
      1,
      0,
      0,
      65535,
      0,
      dst_qp,
      0,
      0,
      0 // psn
    };
    hdr.ib_bth.setValid();
    hdr.ib_reth = {
      msg_vaddr_base,
      rkey,
      0
    };
    hdr.ib_reth.setValid();
  }

  table tab_rdma {
    actions = {
      fill_rdma;
    }
    default_action = fill_rdma(0, 0, 0, 0, 0, 0, 0);
    size = 1;
  }

#ifdef SLIDING
  bit<64> epoch_vaddr_off;

  action set_epoch_vaddr_off(bit<64> val) {
    epoch_vaddr_off = val;
  }

  table tab_epoch_vaddr_off {
    key = {
      epoch : ternary;
    }
    actions = {
      set_epoch_vaddr_off;
    }
    const entries = {
      0 &&& 0x7: set_epoch_vaddr_off(0);
      1 &&& 0x7: set_epoch_vaddr_off(EPOCH_DATA_SIZE);
      2 &&& 0x7: set_epoch_vaddr_off(2 * EPOCH_DATA_SIZE);
      3 &&& 0x7: set_epoch_vaddr_off(3 * EPOCH_DATA_SIZE);
      4 &&& 0x7: set_epoch_vaddr_off(4 * EPOCH_DATA_SIZE);
      5 &&& 0x7: set_epoch_vaddr_off(5 * EPOCH_DATA_SIZE);
      6 &&& 0x7: set_epoch_vaddr_off(6 * EPOCH_DATA_SIZE);
      7 &&& 0x7: set_epoch_vaddr_off(7 * EPOCH_DATA_SIZE);
    }
    size = 8;
  }
#endif

  action addr_tab_hit(bit<64> vaddr) {
    hdr.vaddr.setValid();
#ifdef SLIDING
    hdr.vaddr.vaddr = vaddr + epoch_vaddr_off;
#else
    hdr.vaddr.vaddr = vaddr;
#endif
  }

  @idletime_precision(3)
  table tab_addr {
    key = {
      eg_md.key.dst_ip : exact;
    }
    actions = {
      addr_tab_hit;
      @defaultonly NoAction;
    }
    const default_action = NoAction();
    size = 1 << 16;
    idle_timeout = true;
  }

  Hash<bit<32>>(HashAlgorithm_t.CRC32) hash_icrc_msg;
  Hash<bit<32>>(HashAlgorithm_t.CRC32) hash_icrc_msg_imm;
  Hash<bit<32>>(HashAlgorithm_t.CRC32) hash_icrc_rwr;

  apply {
    bit<16> bf_h_x;
    bit<16> bf_h_y;
    bit<17> reset_idx;

  @stage(0) {
    bf_h_x = hash_bf_x.get(eg_md.key);
    bf_h_y = hash_bf_y.get(eg_md.key);

    if (hdr.eg_op.op[0:0] == 1) { // is_start
      epoch = switch_epoch.execute(0);
    } else if (hdr.eg_op.op[3:3] == 0) { // is_normal (or normal report_key)
      epoch = get_epoch.execute(0);
    } else { // is col or reset
      epoch = get_col_epoch.execute(0);
    }

    if (hdr.eg_op.op[7:7] == 1) { // start_reset
      reset_idx = start_reset_idx.execute(0);
    } else if (hdr.eg_op.op[3:2] == 0b11) { // is_reset
      reset_idx = get_next_reset_idx.execute(0);
    }
  }

    bool filt = hdr.eg_op.op[3:2] == 0 && hdr.tcp.isValid() /* && hdr.tcp.flags == 0x2 */;

    bit<1> bfv_x;
    bit<1> bfv_y;

  @stage(1) {
    if (hdr.eg_op.op[3:2] == 0b11) { // reset
      reset_bf_x.execute(epoch[0:0] ++ reset_idx[15:0]);
      reset_bf_y.execute(epoch[0:0] ++ reset_idx[15:0]);
    } else if (filt) { // normal
      bfv_x = upd_bf_x.execute(epoch[0:0] ++ bf_h_x);
      bfv_y = upd_bf_y.execute(epoch[0:0] ++ bf_h_y);
    }
  }

    bool store_new = false;
    bit<32> key_idx;

  @stage(2) {
    if (filt && (bfv_x == 0 || bfv_y == 0)) { // store new key
      key_idx = get_next_store_key_idx.execute(epoch[0:0]);
      store_new = true;
    } else if (hdr.eg_op.op[3:1] == 0b100) { // col key
      key_idx = get_next_col_key_idx.execute(epoch[0:0]);
    }
#ifdef SLIDING
    tab_epoch_vaddr_off.apply();
#endif
  }

  @stage(3) {
    if (store_new && key_idx != NKEYS) { // store new key
      store_dst_ip.execute(epoch[0:0] ++ key_idx[15:0]);
    } else if (hdr.eg_op.op[3:1] == 0b100 && key_idx != NKEYS) { // col key
      eg_md.key.setValid();
      eg_md.key.dst_ip = get_dst_ip.execute(epoch[0:0] ++ key_idx[15:0]);
    }
  }

  bit<16> cm_h_x;
  bit<16> cm_h_y;
  bit<17> cm_idx_y;

  @stage(4) {
    cm_h_x = hash_cm_x.get(eg_md.key);
    cm_h_y = hash_cm_y.get(eg_md.key);
  }

    if (hdr.eg_op.op[3:2] != 0) {
      hdr.val.setValid();
      tab_rdma.apply();
    }

    // setting resp data
    if (hdr.eg_op.op[3:2] == 0b10) { // col
      if (eg_md.key.isValid()) {
        if (hdr.vaddr.isValid()) {
          hdr.dc_wr_padding = {0};
          hdr.dc_wr_padding.setValid();
          hdr.key.setInvalid();
        } else if (tab_addr.apply().hit) {
          hdr.dc_wr_padding = {0};
          hdr.dc_wr_padding.setValid();
          hdr.key.setInvalid();
        } else {
          hdr.dc_resp = {2, 0};
          hdr.dc_resp.setValid();
          hdr.key = eg_md.key;
          hdr.key.setValid();
        }
      } else {
        eg_dprsr_md.drop_ctl = 0x1;
      }
    } else if (hdr.eg_op.op[3:2] == 0b11) { // reset (done)
      if (reset_idx == 0 || reset_idx == NDATA - 1 || hdr.eg_op.op[4:4] == 1) {
        hdr.dc_resp = {0, 0};
        hdr.dc_resp.setValid();
        hdr.key.setValid();
      } else {
        eg_dprsr_md.drop_ctl = 0x1;
      }
    } else if (hdr.eg_op.op[3:2] == 0b01) { // report key
      hdr.dc_resp = {1, 0};
      hdr.dc_resp.setValid();
      hdr.key.setValid();
    }

    bit<16> cm_v;

  @stage(5) {
    cm_idx_y = epoch[0:0] ++ cm_h_y;
    if (hdr.eg_op.op[3:2] == 0b11 && reset_idx != NDATA) { // reset
      reset_cm_x.execute(epoch[0:0] ++ reset_idx[15:0]);
    } else if (filt) { // normal
      upd_cm_x.execute(epoch[0:0] ++ cm_h_x);
    } else if (hdr.eg_op.op[3:2] == 0b10 && eg_md.key.isValid()) { // col && has key to col
      cm_v = get_cm_x.execute(epoch[0:0] ++ cm_h_x);
    }
  }

  @stage(6) {
    if (hdr.eg_op.op[3:2] == 0b11 && reset_idx != NDATA) { // reset
      reset_cm_y.execute(epoch[0:0] ++ reset_idx[15:0]);
    } else if (filt) { // normal
      upd_cm_y.execute(cm_idx_y);
    } else if (hdr.eg_op.op[3:2] == 0b10 && eg_md.key.isValid()) { // col && has key to col
      cm_v = min(cm_v, get_cm_y.execute(cm_idx_y));
    }
  }

  // @stage(5, 6) {
    if (hdr.dc_resp.isValid()) {
      hdr.ib_bth.psn = get_next_psn.execute(0);
      bit<64> msg_addr = get_next_msg_addr.execute(0);
      hdr.ib_reth.vaddr = hdr.ib_reth.vaddr + msg_addr;
      hdr.ib_reth.dma_len = MSG_LEN;
#ifdef PROC_OVERLAP
      if (msg_addr % MSG_BATCH_SIZE == 0 || hdr.dc_resp.op == 0) {
#else
      if (hdr.dc_resp.op == 0) {
#endif
        hdr.ib_immdt.setValid();
        hdr.ib_immdt.val = msg_addr[31:0];
        hdr.ib_bth.opcode = ib_bth_op_t.RC_RDMA_WRITE_ONLY_WITH_IMM;
        hdr.udp.hdr_length = UDP_HDR_LEN + RWR_HDR_LEN + IMMDT_LEN + MSG_LEN + ICRC_LEN;
        hdr.ipv4.total_len = IP_HDR_LEN + UDP_HDR_LEN + RWR_HDR_LEN + IMMDT_LEN + MSG_LEN + ICRC_LEN;
      } else {
        hdr.ib_bth.opcode = ib_bth_op_t.RC_RDMA_WRITE_ONLY;
        hdr.udp.hdr_length = UDP_HDR_LEN + RWR_HDR_LEN + MSG_LEN + ICRC_LEN;
        hdr.ipv4.total_len = IP_HDR_LEN + UDP_HDR_LEN + RWR_HDR_LEN + MSG_LEN + ICRC_LEN;
      }
    } else if (hdr.vaddr.isValid()) {
      hdr.ib_bth.psn = get_next_psn.execute(0);
      hdr.ib_reth.vaddr = hdr.vaddr.vaddr;
      hdr.ib_reth.dma_len = RWR_LEN;
      hdr.ib_bth.opcode = ib_bth_op_t.RC_RDMA_WRITE_ONLY;
      hdr.udp.hdr_length = UDP_HDR_LEN + RWR_HDR_LEN + RWR_LEN + ICRC_LEN;
      hdr.ipv4.total_len = IP_HDR_LEN + UDP_HDR_LEN + RWR_HDR_LEN + RWR_LEN + ICRC_LEN;
    }
  // }

  @stage(7) {
    hdr.val = {cm_v[7:0], cm_v[15:8]};
    if (hdr.dc_resp.isValid()) {
      if (hdr.ib_immdt.isValid()) {
        bit<32> icrc_val = hash_icrc_msg_imm.get({
          RWR_ICRC_HDR,
          hdr.ib_immdt.val,
          hdr.dc_resp.op,
          hdr.dc_resp.padding,
          hdr.val.v7_0,
          hdr.val.v15_8,
          hdr.key.dst_ip
        });
        hdr.ib_icrc = {icrc_val[7:0], icrc_val[15:8], icrc_val[23:16], icrc_val[31:24]};
        hdr.ib_icrc.setValid();
      } else {
        bit<32> icrc_val = hash_icrc_msg.get({
          RWR_ICRC_HDR,
          hdr.dc_resp.op,
          hdr.dc_resp.padding,
          hdr.val.v7_0,
          hdr.val.v15_8,
          hdr.key.dst_ip
        });
        hdr.ib_icrc = {icrc_val[7:0], icrc_val[15:8], icrc_val[23:16], icrc_val[31:24]};
        hdr.ib_icrc.setValid();
      }
    } else if (hdr.vaddr.isValid()) {
      bit<32> icrc_val = hash_icrc_rwr.get({
        RWR_ICRC_HDR,
        hdr.val.v7_0,
        hdr.val.v15_8,
        hdr.dc_wr_padding.val
      });
      hdr.ib_icrc = {icrc_val[7:0], icrc_val[15:8], icrc_val[23:16], icrc_val[31:24]};
      hdr.ib_icrc.setValid();
      hdr.vaddr.setInvalid();
    }
  }

    // setting egress mirror
    if (hdr.eg_op.op[3:1] == 0b100) { // col keys
      eg_dprsr_md.mirror_type = MIRROR_TYPE_EG;
      eg_md.mirror_session = MIRROR_SESSION_EG_DC;
      eg_md.mirror_hdr.setValid();
      if (key_idx == 0 || (key_idx == NKEYS && hdr.eg_op.op[0:0] == 1)) {
        eg_md.mirror_hdr.eg_op = 8w0b10001100;
      } else if (key_idx == NKEYS) {
        eg_md.mirror_hdr.eg_op = 8w0b00001100;
      } else {
        eg_md.mirror_hdr.eg_op = 8w0b00001000;
      }
    } else if (hdr.eg_op.op[3:2] == 0b11 && reset_idx != NDATA) { // reset
      eg_dprsr_md.mirror_type = MIRROR_TYPE_EG;
      eg_md.mirror_session = MIRROR_SESSION_EG_DC;
      eg_md.mirror_hdr.setValid();
      eg_md.mirror_hdr.eg_op = 8w0b00001100;
    } else if (store_new && key_idx == NKEYS) { // report key
      eg_dprsr_md.mirror_type = MIRROR_TYPE_EG;
      eg_md.mirror_session = MIRROR_SESSION_EG_DC;
      eg_md.mirror_hdr.setValid();
      eg_md.mirror_hdr.eg_op = 8w0b00000110;
      eg_md.mirror_hdr.key_dst_ip = eg_md.key.dst_ip;
    }

    hdr.eg_op.setInvalid();
  }
}

control SwitchEgressDeparser(
    packet_out pkt,
    inout my_hdr_t hdr,
    in eg_md_t eg_md,
    in egress_intrinsic_metadata_for_deparser_t eg_dprsr_md) {

  Checksum() ipv4_checksum;
  Mirror() mirror;

  apply {

    if (eg_md.ipv4_upd) {
      hdr.ipv4.hdr_checksum = ipv4_checksum.update({
        hdr.ipv4.version,
        hdr.ipv4.ihl,
        hdr.ipv4.diffserv,
        hdr.ipv4.total_len,
        hdr.ipv4.identification,
        hdr.ipv4.flags,
        hdr.ipv4.frag_offset,
        hdr.ipv4.ttl,
        hdr.ipv4.protocol,
        hdr.ipv4.src_addr,
        hdr.ipv4.dst_addr
      });
    }

    if (eg_dprsr_md.mirror_type == MIRROR_TYPE_EG) {
      mirror.emit<eg_mirror_h>(eg_md.mirror_session, eg_md.mirror_hdr);
    }

    pkt.emit(hdr);
  }
}


Pipeline(SwitchIngressParser(),
         SwitchIngress(),
         SwitchIngressDeparser(),
         SwitchEgressParser(),
         SwitchEgress(),
         SwitchEgressDeparser()) pipe;

Switch(pipe) main;
