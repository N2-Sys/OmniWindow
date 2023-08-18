#define BLOOM_FILTER_16_DUP(name, h_poly) \
  CRCPolynomial<bit<16>>(h_poly, true, false, false, 0, 0) poly_##name; \
  Hash<bit<16>>(HashAlgorithm_t.CRC16, poly_##name) hash_##name; \
  Register<bit<1>, bit<17>>(1 << 17, 0) reg_##name; \
  RegisterAction<bit<1>, bit<17>, bit<1>>(reg_##name) upd_##name = { \
    void apply(inout bit<1> val, out bit<1> res) { \
      res = val; \
      val = 1; \
    } \
  }; \
  RegisterAction<bit<1>, bit<17>, bit<1>>(reg_##name) reset_##name = { \
    void apply(inout bit<1> val) { \
      val = 0; \
    } \
  };
