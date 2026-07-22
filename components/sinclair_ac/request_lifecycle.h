#pragma once

#include <cassert>
#include <cstdint>

namespace esphome {
namespace sinclair_ac {

// This deliberately contains no ESPHome dependencies so request ownership can
// be tested on a host as well as used by the UART component.
enum class OutstandingRequest : uint8_t { NONE, POLL, COMMAND_APPLY, COMMAND_CLEAR };

struct RequestLifecycle {
  OutstandingRequest outstanding_request{OutstandingRequest::NONE};
  uint32_t outstanding_request_sent_at{0};
  uint32_t polls_sent{0}, poll_responses{0}, poll_response_timeouts{0};
  uint32_t consecutive_poll_timeouts{0}, last_poll_response_ms{0};
  uint32_t min_poll_response_ms{0}, max_poll_response_ms{0};
  uint64_t total_poll_response_ms{0};
  uint32_t command_attempts{0}, command_response_timeouts{0}, command_mismatches{0};

  bool may_send() const { return outstanding_request == OutstandingRequest::NONE; }
  void sent(OutstandingRequest request, uint32_t now) {
    assert(may_send());  // Request ownership is never replaced in flight.
    outstanding_request = request;
    outstanding_request_sent_at = now;
    if (request == OutstandingRequest::POLL) ++polls_sent;
    else ++command_attempts;
  }
  bool acknowledge_report(uint32_t now) {
    if (outstanding_request == OutstandingRequest::NONE) return false;
    if (outstanding_request == OutstandingRequest::POLL) {
      ++poll_responses;
      consecutive_poll_timeouts = 0;
      last_poll_response_ms = now - outstanding_request_sent_at;
      if (poll_responses == 1 || last_poll_response_ms < min_poll_response_ms) min_poll_response_ms = last_poll_response_ms;
      if (last_poll_response_ms > max_poll_response_ms) max_poll_response_ms = last_poll_response_ms;
      total_poll_response_ms += last_poll_response_ms;
    }
    outstanding_request = OutstandingRequest::NONE;
    return true;
  }
  OutstandingRequest timeout(uint32_t now, uint32_t limit_ms) {
    if (outstanding_request == OutstandingRequest::NONE || now - outstanding_request_sent_at < limit_ms) return OutstandingRequest::NONE;
    const auto expired = outstanding_request;
    outstanding_request = OutstandingRequest::NONE;
    if (expired == OutstandingRequest::POLL) { ++poll_response_timeouts; ++consecutive_poll_timeouts; }
    else ++command_response_timeouts;
    return expired;
  }
};

}  // namespace sinclair_ac
}  // namespace esphome
