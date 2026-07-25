#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

#include "../components/gree_oem_boot_probe/rtl_handshake.h"

using esphome::gree_oem_probe::Rtl044HandshakeResponse;
using esphome::gree_oem_probe::decode_rtl_044_handshake_response;

int main() {
  // Exact shape observed from the valid Livo 0x44 response: MID 0x00010001,
  // an empty 16-byte binding code, and VendorInt 1.
  const std::vector<uint8_t> captured{
      0x01, 0x00, 0x01, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x00,
  };

  Rtl044HandshakeResponse decoded;
  assert(decode_rtl_044_handshake_response(captured, decoded));
  assert(decoded.mid == 0x00010001U);
  assert(decoded.vendor_int == 1U);
  assert(!decoded.has_binding_code());

  auto bound = captured;
  bound[4] = 0xA5;
  bound[19] = 0x5A;
  assert(decode_rtl_044_handshake_response(bound, decoded));
  assert(decoded.binding_code.front() == 0xA5);
  assert(decoded.binding_code.back() == 0x5A);
  assert(decoded.has_binding_code());

  auto short_payload = captured;
  short_payload.pop_back();
  assert(!decode_rtl_044_handshake_response(short_payload, decoded));

  auto long_payload = captured;
  long_payload.push_back(0x00);
  assert(!decode_rtl_044_handshake_response(long_payload, decoded));

  return 0;
}
