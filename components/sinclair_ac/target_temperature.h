#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace esphome {
namespace sinclair_ac {
namespace CNT {

// The CNT protocol represents a setpoint as a whole Celsius degree in the
// high nibble: encoded value + 16 C.  Keep this conversion independent of the
// ESPHome climate state so command construction and report verification use
// the same authoritative representation.
constexpr float TARGET_TEMPERATURE_MIN_C = 16.0f;
constexpr float TARGET_TEMPERATURE_MAX_C = 30.0f;
constexpr uint8_t TARGET_TEMPERATURE_MASK = 0xF0;
constexpr uint8_t TARGET_TEMPERATURE_POSITION = 4;

inline float normalize_target_temperature(float requested_c) {
  if (!std::isfinite(requested_c)) return std::numeric_limits<float>::quiet_NaN();
  const float clamped = std::fmax(TARGET_TEMPERATURE_MIN_C,
                                  std::fmin(requested_c, TARGET_TEMPERATURE_MAX_C));
  return std::round(clamped);
}

inline uint8_t encode_target_temperature_field(float requested_c) {
  const float normalized = normalize_target_temperature(requested_c);
  // Callers reject non-finite requests before beginning a transaction.  Keep
  // this fallback deterministic for any future defensive use of the helper.
  if (!std::isfinite(normalized)) return 0;
  return static_cast<uint8_t>(static_cast<uint8_t>(normalized) -
                              static_cast<uint8_t>(TARGET_TEMPERATURE_MIN_C))
         << TARGET_TEMPERATURE_POSITION;
}

inline float decode_target_temperature_field(uint8_t payload_byte) {
  return static_cast<float>(((payload_byte & TARGET_TEMPERATURE_MASK) >>
                             TARGET_TEMPERATURE_POSITION) +
                            static_cast<uint8_t>(TARGET_TEMPERATURE_MIN_C));
}

}  // namespace CNT
}  // namespace sinclair_ac
}  // namespace esphome
