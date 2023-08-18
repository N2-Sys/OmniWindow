#include <core.p4>
#include <tna.p4>

#include "../../p4-include/headers.p4"
#include "../../p4-include/util.p4"

#define N      65536

struct my_header_t {
  ethernet_h ethernet;
  ipv4_h     ipv4;
  udp_h      udp;
  tcp_h      tcp;
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
    tab_forward.apply();
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
#endif

  apply {
    if (eg_md.flowkey.isValid()) {
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
  }
}

control SwitchEgressDeparser(
    packet_out pkt,
    inout my_header_t hdr,
    in eg_metadata_t eg_md,
    in egress_intrinsic_metadata_for_deparser_t eg_dprsr_md) {

  apply {
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
