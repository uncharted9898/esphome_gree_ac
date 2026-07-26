#!/usr/bin/env python3
"""Generate checksum-valid Gree RTL8720CF HVAC UART research frames.

The command-0x03 selector layout and the independent command-0x09 EnergyFlow
request are recovered from the archived RTL8720CF V2 and V3 module firmware.
This utility never opens a serial port or sends a packet; it only builds and
validates byte vectors for review, tests, and explicitly controlled probes.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass


SYNC = (0x7E, 0x7E)
RTL_REPORT_QUERY_SIZE = 29
RTL_REPORT_QUERY_LENGTH = 0x1A
RTL_REPORT_QUERY_COMMAND = 0x03
RTL_REPORT_QUERY_RESERVED_INDEX = 27
RTL_ENERGY_FLOW_QUERY_SIZE = 53
RTL_ENERGY_FLOW_QUERY_LENGTH = 0x32
RTL_ENERGY_FLOW_QUERY_COMMAND = 0x09
RTL_MAC_REPORT_SIZE = 16


@dataclass(frozen=True)
class ExtendedQuery:
    name: str
    primary_selector: int
    extended_selector: int
    expected_response: int


@dataclass(frozen=True)
class RtlDateTimeContext:
    year: int = 0
    month: int = 0
    day: int = 0
    hour: int = 0
    minute: int = 0
    second: int = 0


QUERIES = {
    "status": ExtendedQuery("status", 0x00, 0x00, 0x31),
    "fault_combined": ExtendedQuery("fault_combined", 0x01, 0x00, 0x33),
    "fault_indoor": ExtendedQuery("fault_indoor", 0x02, 0x00, 0x34),
    "fault_outdoor": ExtendedQuery("fault_outdoor", 0x04, 0x00, 0x35),
    "service_page_42": ExtendedQuery("service_page_42", 0x00, 0x01, 0x42),
    "service_page_41": ExtendedQuery("service_page_41", 0x00, 0x02, 0x41),
    "energy_month": ExtendedQuery("energy_month", 0x00, 0x04, 0x40),
}


def checksum(frame_without_checksum: list[int]) -> int:
    if len(frame_without_checksum) < 4 or tuple(frame_without_checksum[:2]) != SYNC:
        raise ValueError("frame must start with 7E 7E")
    return sum(frame_without_checksum[2:]) & 0xFF


def validate_frame(frame: list[int]) -> None:
    if len(frame) < 5:
        raise ValueError("frame is too short")
    if tuple(frame[:2]) != SYNC:
        raise ValueError("bad synchronization bytes")
    if frame[2] + 3 != len(frame):
        raise ValueError(
            f"declared length 0x{frame[2]:02X} does not match {len(frame)} bytes"
        )
    if checksum(frame[:-1]) != frame[-1]:
        raise ValueError("checksum mismatch")


def validate_extended_query(frame: list[int]) -> None:
    """Validate the RTL8720CF command-0x03 report-query envelope."""

    validate_frame(frame)
    if len(frame) != RTL_REPORT_QUERY_SIZE:
        raise ValueError(f"RTL report query must contain {RTL_REPORT_QUERY_SIZE} bytes")
    if frame[2] != RTL_REPORT_QUERY_LENGTH or frame[3] != RTL_REPORT_QUERY_COMMAND:
        raise ValueError("unexpected RTL report-query length or command")
    if frame[RTL_REPORT_QUERY_RESERVED_INDEX] != 0x00:
        raise ValueError("RTL report-query reserved byte must be zero")

    first_context = frame[8:11]
    second_context = frame[11:14]
    expected_first = [0x17, 0x3B, 0x3B] if frame[4] & 0x80 else [0x00] * 3
    expected_second = [0x17, 0x3B, 0x3B] if frame[4] & 0x40 else [0x00] * 3
    if first_context != expected_first:
        raise ValueError("first RTL time-context triple does not match byte-4 bit 7")
    if second_context != expected_second:
        raise ValueError("second RTL time-context triple does not match byte-4 bit 6")


def build_extended_query(
    query: ExtendedQuery,
    *,
    module_state: int = 1,
    state_flags: int = 0,
) -> list[int]:
    """Build the audited 29-byte command-0x03 report request.

    ``state_flags`` occupies full-frame byte 4 bits 7:6. The firmware uses those
    two bits to enable the corresponding HH:MM:SS context triples at bytes
    8..10 and 11..13; the triples become ``17 3B 3B`` (23:59:59). They are not
    RSSI fields. Neutral read-only selector requests leave both triples zero.

    ``module_state`` occupies full-frame byte 26. Full-frame byte 27 is the
    reserved byte omitted by the older 28-byte experiment.
    """

    if not 0 <= module_state <= 0xFF:
        raise ValueError("module_state must fit in one byte")
    if not 0 <= query.primary_selector <= 0x3F:
        raise ValueError("primary selector must fit in byte 4 bits 5:0")
    if not 0 <= query.extended_selector <= 0xFF:
        raise ValueError("extended selector must fit in one byte")
    if state_flags & ~0xC0:
        raise ValueError("state_flags may only use bits 7:6")

    frame = [0x7E, 0x7E, RTL_REPORT_QUERY_LENGTH, RTL_REPORT_QUERY_COMMAND]
    frame.extend([0x00] * (RTL_REPORT_QUERY_SIZE - len(frame)))
    frame[4] = state_flags | query.primary_selector
    if state_flags & 0x80:
        frame[8:11] = [0x17, 0x3B, 0x3B]
    if state_flags & 0x40:
        frame[11:14] = [0x17, 0x3B, 0x3B]
    frame[14] = query.extended_selector
    frame[26] = module_state
    frame[RTL_REPORT_QUERY_RESERVED_INDEX] = 0x00
    frame[-1] = checksum(frame[:-1])
    validate_extended_query(frame)
    return frame


def build_startup_sync(
    *, module_state: int = 1, date_time: RtlDateTimeContext = RtlDateTimeContext()
) -> list[int]:
    """Build the post-0x44 RTL startup command-0x03 frame.

    V2 and V3 send the initialized selector buffer four times after a valid
    identity response. The calendar occupies bytes 15..20. All-zero calendar
    fields are the normal representation before time synchronization.
    """

    if not 0 <= date_time.year <= 0xFFF:
        raise ValueError("year must fit the RTL 12-bit field")
    if not 0 <= date_time.month <= 0x0F:
        raise ValueError("month must fit the RTL four-bit field")
    for name, value in (
        ("day", date_time.day),
        ("hour", date_time.hour),
        ("minute", date_time.minute),
        ("second", date_time.second),
    ):
        if not 0 <= value <= 0xFF:
            raise ValueError(f"{name} must fit in one byte")

    frame = build_extended_query(QUERIES["status"], module_state=module_state)
    frame[15] = (date_time.year >> 4) & 0xFF
    frame[16] = ((date_time.year & 0x0F) << 4) | (date_time.month & 0x0F)
    frame[17] = date_time.day
    frame[18] = date_time.hour
    frame[19] = date_time.minute
    frame[20] = date_time.second
    frame[-1] = checksum(frame[:-1])
    validate_extended_query(frame)
    return frame


def build_energy_flow_query() -> list[int]:
    """Build the independent V2/V3 command-0x09 -> command-0x53 request."""

    frame = [0x00] * RTL_ENERGY_FLOW_QUERY_SIZE
    frame[:4] = [
        0x7E,
        0x7E,
        RTL_ENERGY_FLOW_QUERY_LENGTH,
        RTL_ENERGY_FLOW_QUERY_COMMAND,
    ]
    frame[-1] = checksum(frame[:-1])
    validate_energy_flow_query(frame)
    return frame


def validate_energy_flow_query(frame: list[int]) -> None:
    validate_frame(frame)
    if len(frame) != RTL_ENERGY_FLOW_QUERY_SIZE:
        raise ValueError(
            f"RTL EnergyFlow query must contain {RTL_ENERGY_FLOW_QUERY_SIZE} bytes"
        )
    if (
        frame[2] != RTL_ENERGY_FLOW_QUERY_LENGTH
        or frame[3] != RTL_ENERGY_FLOW_QUERY_COMMAND
    ):
        raise ValueError("unexpected RTL EnergyFlow length or command")
    if any(frame[4:-1]):
        raise ValueError("RTL EnergyFlow request payload must be zero-filled")


def parse_mac(value: str) -> bytes:
    compact = value.replace(":", "").replace("-", "").strip()
    if len(compact) != 12:
        raise ValueError("MAC must contain exactly six bytes")
    try:
        return bytes.fromhex(compact)
    except ValueError as exc:
        raise ValueError("MAC contains non-hexadecimal characters") from exc


def build_full_mac_report(mac: bytes) -> list[int]:
    """Build the audited 16-byte command-0x04 full-MAC solicitation."""

    if len(mac) != 6:
        raise ValueError("MAC must contain exactly six bytes")
    frame = [0x7E, 0x7E, 0x0D, 0x04, 0x07, 0x00, 0x00, 0x00, *mac, 0x00, 0x00]
    frame[-1] = checksum(frame[:-1])
    validate_frame(frame)
    if len(frame) != RTL_MAC_REPORT_SIZE:
        raise ValueError("RTL full-MAC report must contain 16 bytes")
    return frame


def format_hex(frame: list[int]) -> str:
    return " ".join(f"{value:02X}" for value in frame)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="operation", required=True)

    query_parser = sub.add_parser("query", help="generate an audited RTL report query")
    query_parser.add_argument("name", choices=sorted(QUERIES))
    query_parser.add_argument("--module-state", type=lambda value: int(value, 0), default=1)
    query_parser.add_argument("--state-flags", type=lambda value: int(value, 0), default=0)

    sub.add_parser(
        "energy-flow", help="generate the independent command-0x09 EnergyFlow query"
    )

    startup_parser = sub.add_parser(
        "startup-sync", help="generate the post-0x44 RTL command-0x03 startup frame"
    )
    startup_parser.add_argument("--module-state", type=lambda value: int(value, 0), default=1)
    startup_parser.add_argument("--year", type=int, default=0)
    startup_parser.add_argument("--month", type=int, default=0)
    startup_parser.add_argument("--day", type=int, default=0)
    startup_parser.add_argument("--hour", type=int, default=0)
    startup_parser.add_argument("--minute", type=int, default=0)
    startup_parser.add_argument("--second", type=int, default=0)

    mac_parser = sub.add_parser("mac", help="generate the recovered full MAC report")
    mac_parser.add_argument("address")

    args = parser.parse_args()
    if args.operation == "query":
        query = QUERIES[args.name]
        frame = build_extended_query(
            query, module_state=args.module_state, state_flags=args.state_flags
        )
        print(format_hex(frame))
        print(f"expected response command: 0x{query.expected_response:02X}")
    elif args.operation == "energy-flow":
        print(format_hex(build_energy_flow_query()))
        print("expected response command: 0x53")
    elif args.operation == "startup-sync":
        date_time = RtlDateTimeContext(
            args.year, args.month, args.day, args.hour, args.minute, args.second
        )
        print(
            format_hex(
                build_startup_sync(
                    module_state=args.module_state, date_time=date_time
                )
            )
        )
        print("OEM firmware transmits this frame four times after valid 0x44")
    else:
        print(format_hex(build_full_mac_report(parse_mac(args.address))))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
