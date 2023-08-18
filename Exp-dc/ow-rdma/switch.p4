#include <core.p4>
#include <tna.p4>

#include "../../p4-include/headers.p4"
#include "../../p4-include/util.p4"

#define N      65536
#define NKEYS  (32<<10)

/* Choose between different mirror types */
const bit<3> EG_PORT_MIRROR = 2;

/* Session ID */
const MirrorId_t MIRROR_EG_DC = 2;

const bit<16> UDP_PORT_ROCE = 4791;
const bit<16> UDP_PORT_DC = 33669;

#define FLOWKEY_SIZE 13

header flowkey_h {
  ipv4_addr_t dst_ip;
  ipv4_addr_t src_ip;
  bit<16>     dst_port;
  bit<16>     src_port;
  bit<8>      protocol;
}

enum bit<8> ib_bth_op_t {
  RC_SEND_ONLY                = 4,
  RC_RDMA_WRITE_ONLY          = 10,
  RC_RDMA_WRITE_ONLY_WITH_IMM = 11,
  RC_FETCH_ADD                = 20
}

header dc_resp_h {
  bit<16> data;
}

header dc_padding_h {
  bit<8> val;
}

// header dc_rem_h {
//   bit<32> val;
// }

// op[3:3] -- do_notify
// op[2:2] -- do_recirc
// op[1:1] -- has_key
// op[0:0] -- is_normal
header eg_op_h {
  bit<8> op;
}

header eg_op_key_h {
  bit<8> op;

  ipv4_addr_t dst_ip;
  ipv4_addr_t src_ip;
  bit<16>     dst_port;
  bit<16>     src_port;
  bit<8>      protocol;
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

struct my_header_t {
  eg_op_h eg_op;
  flowkey_h dc_req_key_bridge;

  ethernet_h ethernet;
  ipv4_h     ipv4;
  udp_h      udp;
  tcp_h      tcp;

  ib_bth_h   ib_bth;
  ib_reth_h  ib_reth;
  ib_immdt_h ib_immdt;

  flowkey_h    dc_req_key;
  dc_padding_h dc_padding;
  dc_resp_h    dc_resp;

  ib_icrc_h  ib_icrc;
}

struct ig_metadata_t {
}

struct eg_metadata_t {
  bool ipv4_upd;
  flowkey_h flowkey;

  MirrorId_t mirror_session;
  eg_op_h mirror_eg_op;
}


parser SwitchIngressParser(
    packet_in pkt,
    out my_header_t hdr,
    out ig_metadata_t ig_md,
    out ingress_intrinsic_metadata_t ig_intr_md) {

  TofinoIngressParser() tofino_parser;

  state start {
    tofino_parser.apply(pkt, ig_intr_md);
    transition parse_ethernet;
  }

  state parse_ethernet {
    pkt.extract(hdr.ethernet);
    transition select (hdr.ethernet.ether_type) {
      ETHERTYPE_IPV4: parse_ipv4;
      default: accept;
    }
  }

  state parse_ipv4 {
    pkt.extract(hdr.ipv4);
    transition select (hdr.ipv4.ihl, hdr.ipv4.protocol) {
      (5, IP_PROTOCOLS_UDP): parse_udp;
      default: accept;
    }
  }

  state parse_udp {
    pkt.extract(hdr.udp);

    transition select (hdr.udp.src_port, hdr.udp.dst_port) {
      (UDP_PORT_DC, UDP_PORT_DC): parse_eg_op;
      default: accept;
    }
  }

  state parse_eg_op {
    pkt.extract(hdr.eg_op);
    transition select (hdr.eg_op.op[1:1]) {
      0: accept;
      1: parse_dc;
    }
  }

  state parse_dc {
    pkt.extract(hdr.dc_req_key_bridge);
    transition accept;
  }
}


control SwitchIngress(
    inout my_header_t hdr,
    inout ig_metadata_t ig_md,
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
    size = 256;
  }

  apply {
    if (hdr.eg_op.isValid()) {
      send(ig_intr_md.ingress_port);
    } else {
      hdr.eg_op.setValid();
      hdr.eg_op.op = 8w0b00000001;
      tab_forward.apply();
    }
  }
}


control SwitchIngressDeparser(
    packet_out pkt,
    inout my_header_t hdr,
    in ig_metadata_t ig_md,
    in ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md) {

  apply {
    pkt.emit(hdr);
  }
}


parser SwitchEgressParser(
    packet_in pkt,
    out my_header_t hdr,
    out eg_metadata_t eg_md,
    out egress_intrinsic_metadata_t eg_intr_md) {

  TofinoEgressParser() tofino_parser;

  state start {
    tofino_parser.apply(pkt, eg_intr_md);

    eg_md.ipv4_upd = false;
    eg_md.mirror_session = 0;

    pkt.extract(hdr.eg_op);
    transition select(hdr.eg_op.op[1:1]) {
      1: parse_dc;
      0: parse_ethernet;
    }
  }

  state parse_dc {
    pkt.extract(hdr.dc_req_key);
    transition select (hdr.eg_op.op[1:1]) {
      1: accept;
      0: parse_ethernet;
    }
  }

  state parse_ethernet {
    pkt.extract(hdr.ethernet);
    transition select (hdr.ethernet.ether_type) {
      ETHERTYPE_IPV4: parse_ipv4;
      default: accept;
    }
  }

  state parse_ipv4 {
    pkt.extract(hdr.ipv4);

    eg_md.flowkey.setValid();
    eg_md.flowkey.dst_ip = hdr.ipv4.dst_addr;
    eg_md.flowkey.src_ip = hdr.ipv4.src_addr;
    eg_md.flowkey.protocol = hdr.ipv4.protocol;

    transition select (hdr.ipv4.ihl, hdr.ipv4.protocol) {
      (5, IP_PROTOCOLS_UDP): parse_udp;
      (5, IP_PROTOCOLS_TCP): parse_tcp;
      default: accept;
    }
  }

  state parse_udp {
    pkt.extract(hdr.udp);

    eg_md.flowkey.dst_port = hdr.udp.dst_port;
    eg_md.flowkey.src_port = hdr.udp.src_port;

    transition accept;
  }

  state parse_tcp {
    pkt.extract(hdr.tcp);

    eg_md.flowkey.dst_port = hdr.tcp.dst_port;
    eg_md.flowkey.src_port = hdr.tcp.src_port;

    transition accept;
  }
}

control SwitchEgress(
    inout my_header_t hdr,
    inout eg_metadata_t eg_md,
    in egress_intrinsic_metadata_t eg_intr_md,
    in egress_intrinsic_metadata_from_parser_t eg_prsr_md,
    inout egress_intrinsic_metadata_for_deparser_t    eg_dprsr_md,
    inout egress_intrinsic_metadata_for_output_port_t eg_oport_md) {

  CRCPolynomial<bit<16>>(0x8005, true, false, false, 0, 0) poly_0;
  Hash<bit<16>>(HashAlgorithm_t.CRC16, poly_0) hash_0;
  Register<bit<16>, bit<16>>(N, 0) reg_data_0;
  RegisterAction<bit<16>, bit<16>, bit<16>>(reg_data_0) upd_data_0 = {
    void apply(inout bit<16> val, out bit<16> res) {
      res = val;
      val = val + 1;
    }
  };
  RegisterAction<bit<16>, bit<16>, bit<16>>(reg_data_0) get_data_0 = {
    void apply(inout bit<16> val, out bit<16> res) {
      res = val;
    }
  };

#if CM_NUM > 1
  CRCPolynomial<bit<16>>(0x3D65, true, false, false, 0, 0) poly_1;
  Hash<bit<16>>(HashAlgorithm_t.CRC16, poly_1) hash_1;
  Register<bit<16>, bit<16>>(N, 0) reg_data_1;
  RegisterAction<bit<16>, bit<16>, bit<16>>(reg_data_1) upd_data_1 = {
    void apply(inout bit<16> val, out bit<16> res) {
      res = val;
      val = val + 1;
    }
  };
  RegisterAction<bit<16>, bit<16>, bit<16>>(reg_data_1) get_data_1 = {
    void apply(inout bit<16> val, out bit<16> res) {
      res = val;
    }
  };
#endif

#if CM_NUM > 2
  CRCPolynomial<bit<16>>(0x8BB7, true, false, false, 0, 0) poly_2;
  Hash<bit<16>>(HashAlgorithm_t.CRC16, poly_2) hash_2;
  Register<bit<16>, bit<16>>(N, 0) reg_data_2;
  RegisterAction<bit<16>, bit<16>, bit<16>>(reg_data_2) upd_data_2 = {
    void apply(inout bit<16> val, out bit<16> res) {
      res = val;
      val = val + 1;
    }
  };
  RegisterAction<bit<16>, bit<16>, bit<16>>(reg_data_2) get_data_2 = {
    void apply(inout bit<16> val, out bit<16> res) {
      res = val;
    }
  };
#endif

#if CM_NUM > 3
  CRCPolynomial<bit<16>>(0x0589, true, false, false, 0, 0) poly_3;
  Hash<bit<16>>(HashAlgorithm_t.CRC16, poly_3) hash_3;
  Register<bit<16>, bit<16>>(N, 0) reg_data_3;
  RegisterAction<bit<16>, bit<16>, bit<16>>(reg_data_3) upd_data_3 = {
    void apply(inout bit<16> val, out bit<16> res) {
      res = val;
      val = val + 1;
    }
  };
  RegisterAction<bit<16>, bit<16>, bit<16>>(reg_data_3) get_data_3 = {
    void apply(inout bit<16> val, out bit<16> res) {
      res = val;
    }
  };
#endif

  Register<bit<32>, bit<32>>(NKEYS) reg_dst_ip;
  RegisterAction<bit<32>, bit<32>, bit<32>>(reg_dst_ip) store_dst_ip = {
    void apply(inout bit<32> val) {
      val = eg_md.flowkey.dst_ip;
    }
  };
  RegisterAction<bit<32>, bit<32>, bit<32>>(reg_dst_ip) get_dst_ip = {
    void apply(inout bit<32> val, out bit<32> res) {
      res = val;
    }
  };

  Register<bit<32>, bit<32>>(NKEYS) reg_src_ip;
  RegisterAction<bit<32>, bit<32>, bit<32>>(reg_src_ip) store_src_ip = {
    void apply(inout bit<32> val) {
      val = eg_md.flowkey.src_ip;
    }
  };
  RegisterAction<bit<32>, bit<32>, bit<32>>(reg_src_ip) get_src_ip = {
    void apply(inout bit<32> val, out bit<32> res) {
      res = val;
    }
  };

  Register<bit<32>, bit<32>>(NKEYS) reg_ports;
  RegisterAction<bit<32>, bit<32>, bit<32>>(reg_ports) store_ports = {
    void apply(inout bit<32> val) {
      val = eg_md.flowkey.dst_port ++ eg_md.flowkey.src_port;
    }
  };
  RegisterAction<bit<32>, bit<32>, bit<32>>(reg_ports) get_ports = {
    void apply(inout bit<32> val, out bit<32> res) {
      res = val;
    }
  };

  Register<bit<8>, bit<32>>(NKEYS) reg_protocol;
  RegisterAction<bit<8>, bit<32>, bit<8>>(reg_protocol) store_protocol = {
    void apply(inout bit<8> val) {
      val = eg_md.flowkey.protocol;
    }
  };
  RegisterAction<bit<8>, bit<32>, bit<8>>(reg_protocol) get_protocol = {
    void apply(inout bit<8> val, out bit<8> res) {
      res = val;
    }
  };

  Register<bit<32>, bit<0>>(1) reg_store_idx;
  RegisterAction<bit<32>, bit<0>, bit<32>>(reg_store_idx) get_next_store_idx = {
    void apply(inout bit<32> val, out bit<32> res) {
      res = val;
      if (val != NKEYS) {
        val = val + 1;
      }
    }
  };
  RegisterAction<bit<32>, bit<0>, bit<32>>(reg_store_idx) get_next_col_idx = {
    void apply(inout bit<32> val, out bit<32> res) {
      if (val == 0) {
        res = NKEYS;
      } else {
        res = val - 1;
        val = val - 1;
      }
    }
  };


  Register<bit<32>, bit<0>>(1) reg_psn;
  RegisterAction<bit<32>, bit<0>, bit<24>>(reg_psn) get_next_psn = {
    void apply(inout bit<32> val, out bit<24> res) {
      res = (bit<24>)val;
      val = val + 1;
    }
  };

  bit<24> psn;

  Register<bit<32>, bit<0>>(1) reg_msn;
  RegisterAction<bit<32>, bit<0>, bit<64>>(reg_msn) get_next_msn = {
    void apply(inout bit<32> val, out bit<64> res) {
      res = (bit<64>)val;
      val = val + 16;
    }
  };

  bit<64> msn;

  action fill_rdma(mac_addr_t src_mac, ipv4_addr_t src_ip,
            mac_addr_t dst_mac, ipv4_addr_t dst_ip,
            bit<24> dst_qp, bit<32> rkey, bit<64> vaddr_base) {
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
    eg_md.ipv4_upd = true;

    hdr.udp = {
      UDP_PORT_ROCE,
      UDP_PORT_ROCE,
      0, // hdr_length
      0
    };

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
      psn
    };
    hdr.ib_bth.setValid();
    hdr.ib_reth = {
        vaddr_base,
        rkey,
        16
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

  Hash<bit<32>>(HashAlgorithm_t.CRC32) hash_icrc;

  action calc_icrc() {
    bit<32> icrc_val = hash_icrc.get({
      64w0xffffffffffffffff,
      hdr.ipv4.version,
      hdr.ipv4.ihl,
      8w0xff, // hdr.ipv4.diffserv,
      hdr.ipv4.total_len,
      hdr.ipv4.identification,
      hdr.ipv4.flags,
      hdr.ipv4.frag_offset,
      8w0xff, // hdr.ipv4.ttl
      hdr.ipv4.protocol,
      16w0xffff, // hdr.ipv4.hdr_checksum
      hdr.ipv4.src_addr,
      hdr.ipv4.dst_addr,

      hdr.udp.src_port,
      hdr.udp.dst_port,
      hdr.udp.hdr_length,
      16w0xffff, // hdr.udp.checksum,

      hdr.ib_bth.opcode,
      hdr.ib_bth.sol_event,
      hdr.ib_bth.mig_req,
      hdr.ib_bth.pad_cnt,
      hdr.ib_bth.hdr_ver,
      hdr.ib_bth.p_key,
      8w0xff, // bit<8> rsrv0;
      hdr.ib_bth.dst_qp,
      hdr.ib_bth.ack,
      hdr.ib_bth.rsrv1,
      hdr.ib_bth.psn,

      hdr.ib_reth.vaddr,
      hdr.ib_reth.rkey,
      hdr.ib_reth.dma_len,

      hdr.dc_req_key.dst_ip,
      hdr.dc_req_key.src_ip,
      hdr.dc_req_key.dst_port,
      hdr.dc_req_key.src_port,
      hdr.dc_req_key.protocol,
      hdr.dc_padding.val,
      hdr.dc_resp.data
    });
    hdr.ib_icrc = {icrc_val[7:0], icrc_val[15:8], icrc_val[23:16], icrc_val[31:24]};
    hdr.ib_icrc.setValid();
  }

  Hash<bit<32>>(HashAlgorithm_t.CRC32) hash_icrc_imm;

  action calc_icrc_imm() {
    bit<32> icrc_val = hash_icrc_imm.get({
      64w0xffffffffffffffff,
      hdr.ipv4.version,
      hdr.ipv4.ihl,
      8w0xff, // hdr.ipv4.diffserv,
      hdr.ipv4.total_len,
      hdr.ipv4.identification,
      hdr.ipv4.flags,
      hdr.ipv4.frag_offset,
      8w0xff, // hdr.ipv4.ttl
      hdr.ipv4.protocol,
      16w0xffff, // hdr.ipv4.hdr_checksum
      hdr.ipv4.src_addr,
      hdr.ipv4.dst_addr,

      hdr.udp.src_port,
      hdr.udp.dst_port,
      hdr.udp.hdr_length,
      16w0xffff, // hdr.udp.checksum,

      hdr.ib_bth.opcode,
      hdr.ib_bth.sol_event,
      hdr.ib_bth.mig_req,
      hdr.ib_bth.pad_cnt,
      hdr.ib_bth.hdr_ver,
      hdr.ib_bth.p_key,
      8w0xff, // bit<8> rsrv0;
      hdr.ib_bth.dst_qp,
      hdr.ib_bth.ack,
      hdr.ib_bth.rsrv1,
      hdr.ib_bth.psn,

      hdr.ib_reth.vaddr,
      hdr.ib_reth.rkey,
      hdr.ib_reth.dma_len,

      hdr.ib_immdt.val,

      hdr.dc_req_key.dst_ip,
      hdr.dc_req_key.src_ip,
      hdr.dc_req_key.dst_port,
      hdr.dc_req_key.src_port,
      hdr.dc_req_key.protocol,
      hdr.dc_padding.val,
      hdr.dc_resp.data
    });
    hdr.ib_icrc = {icrc_val[7:0], icrc_val[15:8], icrc_val[23:16], icrc_val[31:24]};
    hdr.ib_icrc.setValid();
  }

  apply {
    if (hdr.dc_req_key.isValid()) {
      eg_md.flowkey = hdr.dc_req_key;
      eg_md.flowkey.setValid();
    }

    bit<16> val;

    if (hdr.eg_op.op[0:0] == 0) {
      if (hdr.dc_req_key.isValid()) {
        hdr.dc_resp.setValid();
        bit<16> h0 = hash_0.get(eg_md.flowkey);
        hdr.dc_resp.data = get_data_0.execute(h0);
#if CM_NUM > 1
        bit<16> h1 = hash_1.get(eg_md.flowkey);
        hdr.dc_resp.data = min(hdr.dc_resp.data, get_data_1.execute(h1));
#endif
#if CM_NUM > 2
        bit<16> h2 = hash_2.get(eg_md.flowkey);
        hdr.dc_resp.data = min(hdr.dc_resp.data, get_data_2.execute(h2));
#endif
#if CM_NUM > 3
        bit<16> h3 = hash_3.get(eg_md.flowkey);
        hdr.dc_resp.data = min(hdr.dc_resp.data, get_data_3.execute(h3));
#endif

        hdr.dc_padding.setValid();
        hdr.dc_padding.val = 0;

        msn = get_next_msn.execute(0);
        psn = get_next_psn.execute(0);
        tab_rdma.apply();

        hdr.ib_reth.vaddr = hdr.ib_reth.vaddr + msn;

        if (hdr.eg_op.op[3:3] == 0) {
          hdr.ib_bth.opcode = ib_bth_op_t.RC_RDMA_WRITE_ONLY;
          hdr.udp.hdr_length = 8 + 12 + 16 + 16 + 4;
          hdr.ipv4.total_len = 20 + (8 + 12 + 16 + 16 + 4);
          calc_icrc();
        } else {
          hdr.ib_bth.opcode = ib_bth_op_t.RC_RDMA_WRITE_ONLY_WITH_IMM;
          hdr.udp.hdr_length = 8 + 12 + 16 + 4 + 16 + 4;
          hdr.ipv4.total_len = 20 + (8 + 12 + 16 + 4 + 16 + 4);
          hdr.ib_immdt.setValid();
          hdr.ib_immdt.val = msn[31:0] + 16;
          calc_icrc_imm();
        }
      } else {
        eg_dprsr_md.drop_ctl = 0x1;
      }
    } else if (eg_md.flowkey.isValid()) {
      bit<16> h0 = hash_0.get(eg_md.flowkey);
      val = upd_data_0.execute(h0);

#if CM_NUM > 1
      bit<16> h1 = hash_1.get(eg_md.flowkey);
      val = min(val, upd_data_1.execute(h1));
#endif

#if CM_NUM > 2
      bit<16> h2 = hash_2.get(eg_md.flowkey);
      val = min(val, upd_data_2.execute(h2));
#endif

#if CM_NUM > 3
      bit<16> h3 = hash_3.get(eg_md.flowkey);
      val = min(val, upd_data_3.execute(h3));
#endif
    }

    if (hdr.eg_op.op[2:2] == 1) {
      bit<32> col_idx = get_next_col_idx.execute(0);
      if (col_idx != NKEYS) {
        eg_md.flowkey.setValid();
        eg_md.flowkey.dst_ip = get_dst_ip.execute(col_idx);
        eg_md.flowkey.src_ip = get_src_ip.execute(col_idx);
        bit<32> ports = get_ports.execute(col_idx);
        eg_md.flowkey.dst_port = ports[31:16];
        eg_md.flowkey.src_port = ports[15:0];
        eg_md.flowkey.protocol = get_protocol.execute(col_idx);

        eg_dprsr_md.mirror_type = EG_PORT_MIRROR;
        eg_md.mirror_session = MIRROR_EG_DC;

        eg_md.mirror_eg_op.setValid();
        if (col_idx != 0)
          eg_md.mirror_eg_op.op = 8w0b00000110;
        else
          eg_md.mirror_eg_op.op = 8w0b00001110;
      }
    } else if (hdr.eg_op.op[0:0] == 1 && eg_md.flowkey.isValid() && val == 0) {
      bit<32> store_idx = get_next_store_idx.execute(0);
      if (store_idx != NKEYS) {
        store_dst_ip.execute(store_idx);
        store_src_ip.execute(store_idx);
        store_ports.execute(store_idx);
        store_protocol.execute(store_idx);
      }
    }

    hdr.eg_op.setInvalid();
  }
}

control SwitchEgressDeparser(
    packet_out pkt,
    inout my_header_t hdr,
    in eg_metadata_t eg_md,
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

    if (eg_dprsr_md.mirror_type == EG_PORT_MIRROR) {
      mirror.emit<eg_op_key_h>(eg_md.mirror_session, {
        eg_md.mirror_eg_op.op,
        eg_md.flowkey.dst_ip,
        eg_md.flowkey.src_ip,
        eg_md.flowkey.dst_port,
        eg_md.flowkey.src_port,
        eg_md.flowkey.protocol});
    }

    pkt.emit(hdr);
  }
}


Pipeline(
    SwitchIngressParser(),
    SwitchIngress(),
    SwitchIngressDeparser(),
    SwitchEgressParser(),
    SwitchEgress(),
    SwitchEgressDeparser()) pipe;

Switch(pipe) main;
