#define COUNT_MIN_16_DUP(name, h_poly) \
  CRCPolynomial<bit<16>>(h_poly, true, false, false, 0, 0) poly_##name; \
  Hash<bit<16>>(HashAlgorithm_t.CRC16, poly_##name) hash_##name; \
  Register<bit<16>, bit<17>>(1 << 17, 0) reg_##name; \
  RegisterAction<bit<16>, bit<17>, bit<16>>(reg_##name) upd_##name = { \
    void apply(inout bit<16> val, out bit<16> res) { \
      res = val; \
      val = val + 1; \
    } \
  }; \
  RegisterAction<bit<16>, bit<17>, bit<16>>(reg_##name) reset_##name = { \
    void apply(inout bit<16> val) { \
      val = 0; \
    } \
  }; \
  RegisterAction<bit<16>, bit<17>, bit<16>>(reg_##name) get_##name = { \
    void apply(inout bit<16> val, out bit<16> res) { \
      res = val; \
    } \
  };
