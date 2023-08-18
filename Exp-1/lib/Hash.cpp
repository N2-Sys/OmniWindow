#include "Hash.hpp"
#include <crcutil/generic_crc.h>

#define TOPBIT (((uint32_t)1) << 31)

static uint32_t reflect(uint32_t VF_dato, uint8_t VF_nBits) {
  uint32_t VP_reflection = 0;

  for (uint8_t VP_Pos_bit = 0; VP_Pos_bit < VF_nBits; VP_Pos_bit++) {
    if ((VF_dato & 1) == 1) {
      VP_reflection |= (((uint32_t)1) << ((VF_nBits - 1) - VP_Pos_bit));
    }
    VF_dato = (VF_dato >> 1);
  }
  return (VP_reflection);
}

#define reflect_DATA(_DATO) ((uint8_t)(reflect((_DATO), 8) & 0xFF))
#define reflect_CRCTableValue(_CRCTableValue)                                  \
  ((uint32_t)reflect((_CRCTableValue), 32))

static uint32_t crc_ObtenValorDeTabla_Reversed(uint8_t VP_Pos_Tabla,
                                               uint32_t polynomial) {
  uint32_t VP_CRCTableValue = 0;

  VP_CRCTableValue = ((uint32_t)reflect_DATA(VP_Pos_Tabla)) << 24;

  for (uint8_t VP_Pos_bit = 0; VP_Pos_bit < 8; VP_Pos_bit++) {
    if (VP_CRCTableValue & TOPBIT) {
      VP_CRCTableValue = (VP_CRCTableValue << 1) ^ polynomial;
    } else {
      VP_CRCTableValue = (VP_CRCTableValue << 1);
    }
  }
  return (reflect_CRCTableValue(VP_CRCTableValue));
}

static uint32_t crc_body_reversed_true(const uint8_t *data, uint16_t len,
                                uint32_t code, uint32_t init,
                                uint32_t final_xor) {
  for (int16_t VP_bytes = 0; VP_bytes < len; VP_bytes++) {
    init = (init >> 8) ^ crc_ObtenValorDeTabla_Reversed(
                             ((uint8_t)(init & 0xFF)) ^ data[VP_bytes], code);
  }

  return (init ^ final_xor);
}

uint32_t crc32(const void *data, uint16_t len) {
  return crc_body_reversed_true((const uint8_t *)data, len, 0x04C11DB7,
                               0xFFFFFFFF, 0xFFFFFFFF);
}

uint32_t crc_32c(uint8_t* data, uint16_t len) {
  return crc_body_reversed_true(data, len, 0x1EDC6F41, 0xFFFFFFFF, 0xFFFFFFFF);
}

uint32_t crc_32d(uint8_t* data, uint16_t len) {
  return crc_body_reversed_true(data, len, 0xA833982B, 0xFFFFFFFF, 0xFFFFFFFF);
}

uint32_t jamcrc(uint8_t* data, uint16_t len) {
  return crc_body_reversed_true(data, len, 0x04C11DB7, 0xFFFFFFFF, 0);
}

static uint32_t crc_ObtenValorDeTabla(uint8_t VP_Pos_Tabla,
                                      uint32_t polynomial) {
  uint32_t VP_CRCTableValue = ((uint32_t)VP_Pos_Tabla) << 24;

  for (uint8_t VP_Pos_bit = 0; VP_Pos_bit < 8; VP_Pos_bit++) {
    if (VP_CRCTableValue & TOPBIT) {
      VP_CRCTableValue = (VP_CRCTableValue << 1) ^ polynomial;
    } else {
      VP_CRCTableValue = (VP_CRCTableValue << 1);
    }
  }
  return (VP_CRCTableValue);
}

uint32_t crc_body_reversed_false(uint8_t* data, uint16_t len, uint32_t code,
                                 uint32_t init, uint32_t final_xor) {
  for (int16_t VP_bytes = 0; VP_bytes < len; VP_bytes++) {
    init = (init << 8) ^
           crc_ObtenValorDeTabla(
               ((uint8_t)((init >> 24) & 0xFF)) ^ data[VP_bytes], code);
  }

  return (init ^ final_xor);
}

uint32_t xfer(uint8_t* data, uint16_t len) {
  return crc_body_reversed_false(data, len, 0xAF, 0, 0);
}

uint32_t posix(uint8_t* data, uint16_t len) {
  return crc_body_reversed_false(data, len, 0x04C11DB7, 0, 0xFFFFFFFF);
}

uint32_t crc_32_bzip2(uint8_t* data, uint16_t len) {
  return crc_body_reversed_false(data, len, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF);
}

uint32_t crc_32_mpeg(uint8_t* data, uint16_t len) {
  return crc_body_reversed_false(data, len, 0x04C11DB7, 0xFFFFFFFF, 0);
}

uint32_t crc_32q(uint8_t* data, uint16_t len) {
  return crc_body_reversed_false(data, len, 0x814141AB, 0, 0);
}

static crcutil::GenericCrc<uint32_t, uint32_t, uint32_t, 4> c[9] = {
  {0xEDB88320, 32, true},
  {0x82F63B78, 32, true},
  {0xD419CC15, 32, true},
  {},
  {},
  {},
  {},
  {},
  {0xEDB88320, 32, false}
};

uint32_t select_crc(int hashid, const void *data, uint16_t len) {
  switch (hashid) {
    case 0: case 1: case 2:
      return c[hashid].CrcDefault(data, len, 0);
    case 8:
      return c[hashid].CrcDefault(data, len, ~0U);
    case 3:
      return crc_32q((uint8_t *)data, len);
    case 4:
      return crc_32_bzip2((uint8_t *)data, len);
    case 5:
      return crc_32_mpeg((uint8_t *)data, len);
    case 6:
      return posix((uint8_t *)data, len);
    case 7:
      return xfer((uint8_t *)data, len);
    default:
      throw std::out_of_range("Unsupported hash id");
  }
}
