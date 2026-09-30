#include <cassert>
#include <cstdint>
#include <vector>

#include "../components/gree_wired_rs485/wired_protocol.h"

using namespace esphome::gree_wired_rs485::protocol;

static std::vector<uint8_t> reference_00_ff() {
  return {
      0x7E, 0x7E, 0x00, 0xFF, 0x11, 0x0E, 0x00, 0x00, 0x02, 0x01,
      0x89, 0x8A, 0xBE, 0x47, 0x00, 0x80, 0x00, 0x00, 0x00, 0x99,
  };
}

static std::vector<uint8_t> reference_ff_00() {
  return {
      0x7E, 0x7E, 0xFF, 0x00, 0x11, 0x15, 0x0C, 0x30, 0x83,
      0x01, 0x14, 0x7E, 0x22, 0x10, 0xE0, 0xE0, 0x08, 0x00,
      0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17,
  };
}

static std::vector<uint8_t> reference_ff_40() {
  return {
      0x7E, 0x7E, 0xFF, 0x40, 0x11, 0x16, 0x0C, 0x30, 0x83,
      0x79, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x07,
  };
}

int main() {
  ParsedFrame parsed;

  const auto a = reference_00_ff();
  assert(parse_frame(a, parsed));
  assert(parsed.source == 0x00);
  assert(parsed.destination == 0xFF);
  assert(parsed.message_type == 0x11);
  assert(parsed.body_length == 0x0E);
  assert(parsed.payload.size() == 13);
  assert(parsed.route == RouteKind::ROUTE_00_FF);
  assert(parsed.frame_class == FrameClass::REFERENCE_LAYOUT);
  assert(parsed.reference_body_length);
  assert(xor_bytes(a) == 0);

  const auto b = reference_ff_00();
  assert(parse_frame(b, parsed));
  assert(parsed.route == RouteKind::ROUTE_FF_00);
  assert(parsed.body_length == 0x15);
  assert(parsed.payload.size() == 20);
  assert(parsed.frame_class == FrameClass::REFERENCE_LAYOUT);
  assert(xor_bytes(b) == 0);

  const auto c = reference_ff_40();
  assert(parse_frame(c, parsed));
  assert(parsed.route == RouteKind::ROUTE_FF_40);
  assert(parsed.body_length == 0x16);
  assert(parsed.payload.size() == 21);
  assert(parsed.frame_class == FrameClass::REFERENCE_LAYOUT);
  assert(xor_bytes(c) == 0);

  // A known route with a new body length is retained for R32/Vireo discovery
  // rather than rejected as incompatible with older controller captures.
  auto route_variant = a;
  route_variant.insert(route_variant.end() - 1, 0x55);
  route_variant[5] = 0x0F;
  route_variant.back() = 0;
  uint8_t checksum = 0;
  for (const auto value : route_variant) checksum ^= value;
  route_variant.back() ^= checksum;
  assert(xor_bytes(route_variant) == 0);
  assert(parse_frame(route_variant, parsed));
  assert(parsed.route == RouteKind::ROUTE_00_FF);
  assert(parsed.frame_class == FrameClass::KNOWN_ROUTE_VARIANT);
  assert(!parsed.reference_body_length);

  auto bad_checksum = a;
  bad_checksum.back() ^= 0x01;
  assert(!parse_frame(bad_checksum, parsed));

  auto bad_length = a;
  bad_length[5] = 0x0D;
  assert(!parse_frame(bad_length, parsed));

  auto unknown_route = a;
  unknown_route[2] = 0x22;
  unknown_route.back() = 0;
  checksum = 0;
  for (const auto value : unknown_route) checksum ^= value;
  unknown_route.back() ^= checksum;
  assert(parse_frame(unknown_route, parsed));
  assert(parsed.route == RouteKind::UNKNOWN);
  assert(parsed.frame_class == FrameClass::UNKNOWN_ROUTE);

  FrameAssembler assembler;
  std::vector<uint8_t> completed;
  for (size_t i = 0; i + 1 < b.size(); ++i) {
    assert(assembler.push(b[i], completed) == AssembleResult::NONE);
  }
  assert(assembler.active());
  assert(assembler.push(b.back(), completed) == AssembleResult::FRAME_READY);
  assert(completed == b);
  assert(!assembler.active());

  // Noise before sync is ignored.
  assert(assembler.push(0x00, completed) == AssembleResult::NONE);
  assert(assembler.push(0x7E, completed) == AssembleResult::NONE);
  assert(assembler.push(0x00, completed) == AssembleResult::NONE);
  assert(!assembler.active());

  // Impossible lengths are dropped before they can grow the capture buffer.
  const std::vector<uint8_t> invalid_header{0x7E, 0x7E, 0x00, 0xFF, 0x11, 0x00};
  for (size_t i = 0; i + 1 < invalid_header.size(); ++i) {
    assembler.push(invalid_header[i], completed);
  }
  assert(assembler.push(invalid_header.back(), completed) ==
         AssembleResult::INVALID_LENGTH);
  assert(!assembler.active());

  return 0;
}
