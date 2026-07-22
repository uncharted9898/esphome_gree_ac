#include "../components/sinclair_ac/protocol_frame.h"

#include <cassert>

using namespace sinclair_ac_protocol;

static std::vector<uint8_t> frame(uint8_t command, size_t payload_length) {
  std::vector<uint8_t> raw{SYNC, SYNC, static_cast<uint8_t>(payload_length + 2), command};
  raw.resize(payload_length + 4);

  uint8_t checksum = 0;
  for (size_t i = 2; i < raw.size(); ++i) checksum += raw[i];
  raw.push_back(checksum);
  return raw;
}

int main() {
  ParsedFrame parsed;

  const auto known = frame(0x31, 43);
  assert(parse(known, parsed) == Result::VALID_KNOWN);
  assert(parsed.payload_length == 43);
  assert(parsed.raw == known);

  const auto transmitted = frame(0x01, 45);
  assert(transmitted[2] == 0x2F);
  assert(transmitted.size() == 50);

  const std::vector<uint8_t> livo_report{
      0x7E, 0x7E, 0x31, 0x31,
      0x04, 0x00, 0x40, 0x00, 0x90, 0x80, 0x06, 0xC2, 0x00, 0x00, 0x00, 0x08,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x46, 0x00, 0x00,
      0x15,
  };
  assert(livo_report[2] == 0x31);
  assert(livo_report.size() == 52);
  assert(parse(livo_report, parsed) == Result::VALID_KNOWN);
  assert(parsed.declared_length == 49);
  assert(parsed.command == 0x31);
  assert(parsed.payload_length == 47);
  assert(parsed.received_checksum == 0x15);
  assert(parsed.calculated_checksum == 0x15);

  assert(parse(frame(0x44, 1), parsed) == Result::VALID_UNKNOWN);
  auto bad = known;
  bad.back()++;
  assert(parse(bad, parsed) == Result::CHECKSUM);
  const auto short31 = frame(0x31, 4);
  assert(parse(short31, parsed) == Result::VALID_KNOWN);
  assert(parsed.payload_length == 4);
  assert(parse(frame(0x31, 80), parsed) == Result::VALID_KNOWN);
  const auto max = frame(0x31, 195);
  assert(max.size() == 200);
  assert(parse(max, parsed) == Result::VALID_KNOWN);
  assert(parse({SYNC, SYNC, 3, 0x31}, parsed) == Result::TOO_SHORT);
  auto length = known;
  length[2]--;
  assert(parse(length, parsed) == Result::LENGTH);
  return 0;
}
