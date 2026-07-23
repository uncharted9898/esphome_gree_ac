#!/usr/bin/env python3
"""Generate checksum-valid Gree OEM UART research frames.

The layouts in this tool come from static analysis of the public
CS532AX/MT7687 OEM firmware. It does not access a serial port and never sends a
packet. Its output is intended for review, tests, and explicitly controlled
hardware probes.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass


SYNC = (0x7E, 0x7E)


@dataclass(frozen=True)
class ExtendedQuery:
    name: str
    primary_selector: int
    extended_selector: int
    expected_response: int


QUERIES = {
    "status": ExtendedQuery("status", 0x00, 0x00, 0x31),
    "fault_combined": ExtendedQuery("fault_combined", 0x01, 0x00, 0x33),
    "fault_indoor": ExtendedQuery("fault_indoor", 0x02, 0x00, 0x34),
    "fault_outdoor": ExtendedQuery("fault_outdoor", 0x04, 0x00, 0x35),
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


def build_extended_query(
    query: ExtendedQuery,
    *,
    module_state: int = 1,
    state_flags: int = 0,
) -> list[int]:
    """Build the recovered length-0x19 command-0x03 request.

    state_flags occupies full-frame byte 4 bits 7:6. module_state occupies full
    frame byte 26. The defaults reproduce a neutral, checksum-valid research
    vector rather than pretending to know the original module's live Wi-Fi
    state.
    """

    if not 0 <= module_state <= 0xFF:
        raise ValueError("module_state must fit in one byte")
    if state_flags & ~0xC0:
        raise ValueError("state_flags may only use bits 7:6")

    frame = [0x7E, 0x7E, 0x19, 0x03] + [0x00] * 23 + [0x00]
    frame[4] = state_flags | query.primary_selector
    frame[10] = 0x3B
    frame[13] = 0x3B
    frame[14] = query.extended_selector
    frame[26] = module_state
    frame[27] = checksum(frame[:-1])
    validate_frame(frame)
    return frame


def parse_mac(value: str) -> bytes:
    compact = value.replace(":", "").replace("-", "").strip()
    if len(compact) != 12:
        raise ValueError("MAC must contain exactly six bytes")
    try:
        return bytes.fromhex(compact)
    except ValueError as exc:
        raise ValueError("MAC contains non-hexadecimal characters") from exc


def build_full_mac_report(mac: bytes) -> list[int]:
    if len(mac) != 6:
        raise ValueError("MAC must contain exactly six bytes")
    frame = [0x7E, 0x7E, 0x0C, 0x04, 0x07, 0x00, 0x00, 0x00, *mac, 0x00]
    frame[-1] = checksum(frame[:-1])
    validate_frame(frame)
    return frame


def format_hex(frame: list[int]) -> str:
    return " ".join(f"{value:02X}" for value in frame)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="operation", required=True)

    query_parser = sub.add_parser("query", help="generate a recovered extended query")
    query_parser.add_argument("name", choices=sorted(QUERIES))
    query_parser.add_argument("--module-state", type=lambda value: int(value, 0), default=1)
    query_parser.add_argument("--state-flags", type=lambda value: int(value, 0), default=0)

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
    else:
        print(format_hex(build_full_mac_report(parse_mac(args.address))))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
