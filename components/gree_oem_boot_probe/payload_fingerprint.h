#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace esphome {
namespace gree_oem_probe {

inline uint32_t payload_fnv1a(const std::vector<uint8_t> &payload) {
  uint32_t hash = 2166136261UL;
  for (const uint8_t value : payload) {
    hash ^= value;
    hash *= 16777619UL;
  }
  return hash;
}

inline std::string payload_changed_indices(const std::vector<uint8_t> &baseline,
                                           const std::vector<uint8_t> &current,
                                           size_t maximum_entries = 24) {
  std::string result;
  size_t emitted = 0;
  const size_t common_size = std::min(baseline.size(), current.size());

  for (size_t index = 0; index < common_size; ++index) {
    if (baseline[index] == current[index]) continue;
    if (emitted >= maximum_entries) {
      if (!result.empty()) result += ',';
      result += "...";
      return result;
    }
    char token[24];
    std::snprintf(token, sizeof(token), "%u", static_cast<unsigned>(index));
    if (!result.empty()) result += ',';
    result += token;
    ++emitted;
  }

  if (baseline.size() != current.size()) {
    char token[48];
    std::snprintf(token, sizeof(token), "size:%u->%u",
                  static_cast<unsigned>(baseline.size()),
                  static_cast<unsigned>(current.size()));
    if (!result.empty()) result += ',';
    result += token;
  }

  return result.empty() ? "none" : result;
}

}  // namespace gree_oem_probe
}  // namespace esphome
