#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace sinclair_ac_protocol {
constexpr uint8_t SYNC = 0x7E;
constexpr size_t MAX_FRAME = 200;
struct ParsedFrame { std::vector<uint8_t> raw; uint8_t declared_length{0}; uint8_t command{0}; size_t payload_offset{4}; size_t payload_length{0}; uint8_t received_checksum{0}; uint8_t calculated_checksum{0}; };
enum class Result { VALID_KNOWN, VALID_UNKNOWN, TOO_SHORT, LENGTH, CHECKSUM };
inline Result parse(const std::vector<uint8_t> &raw, ParsedFrame &frame) {
 frame = ParsedFrame{}; frame.raw=raw; if(raw.size()<5) return Result::TOO_SHORT;
 frame.declared_length=raw[2]; if(raw[0]!=SYNC||raw[1]!=SYNC||static_cast<size_t>(raw[2]) + 2 != raw.size()) return Result::LENGTH;
 frame.command=raw[3]; frame.payload_length=raw.size()-5; frame.received_checksum=raw.back();
 for(size_t i=2;i+1<raw.size();++i) frame.calculated_checksum=static_cast<uint8_t>(frame.calculated_checksum+raw[i]);
 if(frame.calculated_checksum!=frame.received_checksum) return Result::CHECKSUM;
 return frame.command==0x31 ? Result::VALID_KNOWN : Result::VALID_UNKNOWN;
}
}
