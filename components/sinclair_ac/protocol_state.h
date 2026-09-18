#pragma once

enum class ACState {
    Initializing, /* no data for quite a long time */
    Ready,        /* AC talking to us */
};

enum class ACUpdate {
    NoUpdate,    /* no parameters changed - normally process data, static flag set */
    UpdateStart, /* start update with 0xAF and cleared static flag */
    UpdateClear, /* update without 0xAF and cleared static flag */
};

inline const char *protocol_state_after_transmit(ACUpdate update, ACState state) {
    if (update == ACUpdate::UpdateStart) return "command_apply_waiting";
    if (update == ACUpdate::UpdateClear) return "command_clear_waiting";
    if (state != ACState::Ready) return "waiting_for_first_poll_response";

    return nullptr;
}
