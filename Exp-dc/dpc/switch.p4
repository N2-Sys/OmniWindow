#include <core.p4>
#include <tna.p4>

#include "../../p4-include/headers.p4"
#include "../../p4-include/util.p4"

#define N      65536

const bit<3>     MIRROR_TYPE_EG       = 2;
const MirrorId_t MIRROR_SESSION_EG_DC = 2;

const bit<16> UDP_PORT_DC = 33669;

// op[1:1] -- is_start
// op[0:0] -- normal(0), col(1)
header eg_op_h {
  bit<8> op;
}

header dc_req_h {
  bit<16> idx;
}

header dc_resp_h {
  bit<16> data_0;
#if CM_NUM > 1
  bit<16> data_1;
#endif
#if CM_NUM > 2
  bit<16> data_2;
#endif
#if CM_NUM > 3
  bit<16> data_3;
#endif
}

struct my_header_t {
  eg_op_h    eg_op;

  ethernet_h ethernet;
  ipv4_h     ipv4;
  udp_h      udp;
  tcp_h      tcp;

  dc_req_h   dc_req;
  dc_resp_h  dc_resp;
}

header flowkey_h {
  ipv4_addr_t dst_ip;
  ipv4_addr_t src_ip;
  bit<16>     dst_port;
  bit<16>     src_port;
  bit<8>      protocol;
}

struct ig_metadata_t {
}

struct eg_metadata_t {
  MirrorId_t mirror_session;
  eg_op_h mirror_eg_op;

  flowkey_h flowkey;
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
    transition select (hdr.udp.dst_port) {
      UDP_PORT_DC: parse_eg_op;
      default: accept;
    }
  }

  state parse_eg_op {
    pkt.extract(hdr.eg_op);
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
      // hdr.ethernet.setInvalid();
      // hdr.ipv4.setInvalid();
      // hdr.udp.setInvalid();
    } else {
      hdr.eg_op.setValid();
      hdr.eg_op.op = 8w00000000;
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
    eg_md.mirror_session = 0;

    pkt.extract(hdr.eg_op);
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

  Register<bit<32>, bit<0>>(1, 0) reg_col_idx;
  RegisterAction<bit<32>, bit<0>, bit<32>>(reg_col_idx) reset_col_idx = {
    void apply(inout bit<32> val, out bit<32> res) {
      res = 0;
      val = 1;
    }
  };
  RegisterAction<bit<32>, bit<0>, bit<32>>(reg_col_idx) get_next_col_idx = {
    void apply(inout bit<32> val, out bit<32> res) {
      res = val;
      if (val != N) {
        val = val + 1;
      }
    }
  };

  apply {
    if (hdr.eg_op.op[0:0] == 1) {
      bit<32> col_idx;
      if (hdr.eg_op.op[1:1] == 1) {
        col_idx = reset_col_idx.execute(0);
      } else {
        col_idx = get_next_col_idx.execute(0);
      }

      if (col_idx == N) {
        eg_dprsr_md.drop_ctl = 0x1;
      } else {
        hdr.dc_req.setValid();
        hdr.dc_req.idx = col_idx[15:0];
        eg_dprsr_md.mirror_type = MIRROR_TYPE_EG;
        eg_md.mirror_session = MIRROR_SESSION_EG_DC;
        eg_md.mirror_eg_op.setValid();
        eg_md.mirror_eg_op.op = 0b00000001;
      }
    }

    if (hdr.dc_req.isValid()) {
      hdr.dc_resp.setValid();
      hdr.dc_resp.data_0 = get_data_0.execute(hdr.dc_req.idx);
#if CM_NUM > 1
      hdr.dc_resp.data_1 = get_data_1.execute(hdr.dc_req.idx);
#endif
#if CM_NUM > 2
      hdr.dc_resp.data_2 = get_data_2.execute(hdr.dc_req.idx);
#endif
#if CM_NUM > 3
      hdr.dc_resp.data_3 = get_data_3.execute(hdr.dc_req.idx);
#endif

      hdr.ipv4.total_len = 20 + 8 + 2 + 2 * CM_NUM;
      hdr.udp.hdr_length = 8 + 2 + 2 * CM_NUM;

    } else if (hdr.eg_op.op[0:0] == 0 && eg_md.flowkey.isValid()) {
      bit<16> h0 = hash_0.get(eg_md.flowkey);
      upd_data_0.execute(h0);

#if CM_NUM > 1
      bit<16> h1 = hash_1.get(eg_md.flowkey);
      upd_data_1.execute(h1);
#endif

#if CM_NUM > 2
      bit<16> h2 = hash_2.get(eg_md.flowkey);
      upd_data_2.execute(h2);
#endif

#if CM_NUM > 3
      bit<16> h3 = hash_3.get(eg_md.flowkey);
      upd_data_3.execute(h3);
#endif
    }

    hdr.eg_op.setInvalid();
  }
}

control SwitchEgressDeparser(
    packet_out pkt,
    inout my_header_t hdr,
    in eg_metadata_t eg_md,
    in egress_intrinsic_metadata_for_deparser_t eg_dprsr_md) {

  Mirror() mirror;

  apply {
    if (eg_dprsr_md.mirror_type == MIRROR_TYPE_EG) {
      mirror.emit<eg_op_h>(eg_md.mirror_session, eg_md.mirror_eg_op);
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
