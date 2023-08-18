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

const bit<16> UDP_PORT_DC = 33669;

#define FLOWKEY_SIZE 13

header flowkey_h {
  ipv4_addr_t dst_ip;
  ipv4_addr_t src_ip;
  bit<16>     dst_port;
  bit<16>     src_port;
  bit<8>      protocol;
}

header dc_resp_h {
  bit<16> data;
}

header dc_rem_h {
  bit<32> val;
}

// op[2:2] -- do_recirc
// op[1:1] -- has_key
// op[0:0] -- is_normal
header eg_op_h {
  bit<8> op;
}

header eg_op_key_rem_h {
  bit<8> op;

  ipv4_addr_t dst_ip;
  ipv4_addr_t src_ip;
  bit<16>     dst_port;
  bit<16>     src_port;
  bit<8>      protocol;

  bit<32> rem;
}

struct my_header_t {
  eg_op_h eg_op;
  flowkey_h dc_req_key_bridge;
  dc_rem_h  dc_rem_bridge;

  ethernet_h ethernet;
  ipv4_h     ipv4;
  udp_h      udp;
  tcp_h      tcp;

  dc_rem_h  dc_rem;
  dc_resp_h dc_resp;
  flowkey_h dc_req_key;
}

struct ig_metadata_t {
}

struct eg_metadata_t {
  flowkey_h flowkey;

  MirrorId_t mirror_session;
  eg_op_h mirror_eg_op;
  dc_rem_h mirror_dc_rem;
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
    pkt.extract(hdr.dc_rem_bridge);
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

    eg_md.mirror_session = 0;

    pkt.extract(hdr.eg_op);
    transition select(hdr.eg_op.op[1:1]) {
      1: parse_dc;
      0: parse_ethernet;
    }
  }

  state parse_dc {
    pkt.extract(hdr.dc_req_key);
    pkt.extract(hdr.dc_rem);
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

        hdr.ipv4.total_len = 20 + 8 + 4 + 2 + FLOWKEY_SIZE;
        hdr.udp.hdr_length = 8 + 2 + 4 + 2 + FLOWKEY_SIZE;
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
        eg_md.mirror_eg_op.op = 8w0b00000110;

        eg_md.mirror_dc_rem.setValid();
        eg_md.mirror_dc_rem.val = col_idx;
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

  Mirror() mirror;

  apply {
    if (eg_dprsr_md.mirror_type == EG_PORT_MIRROR) {
      mirror.emit<eg_op_key_rem_h>(eg_md.mirror_session, {
        eg_md.mirror_eg_op.op,
        eg_md.flowkey.dst_ip,
        eg_md.flowkey.src_ip,
        eg_md.flowkey.dst_port,
        eg_md.flowkey.src_port,
        eg_md.flowkey.protocol,
        eg_md.mirror_dc_rem.val});
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
