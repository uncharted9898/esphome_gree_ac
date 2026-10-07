#!/usr/bin/env python3
"""Summarize passive XE71/XE72 COM-MANUAL discovery logs.

This parser is intentionally conservative.  It reports UART-profile byte
counts, validated legacy-frame evidence, and raw GPIO edge cadence without
promoting any of them into XE71 protocol semantics.
"""

from __future__ import annotations

import argparse
import json
import re
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Optional


SCAN_RE = re.compile(
    r"SCAN profile=(?P<profile>\S+) bytes=(?P<bytes>\d+) "
    r"legacy_valid=(?P<valid>\d+)"
)
HEALTH_RE = re.compile(r"\bHEALTH\s+(?P<body>.+)$")
KV_RE = re.compile(r"(?P<key>[A-Za-z0-9_]+)=(?P<value>\S+)")
TX_RE = re.compile(
    r"\b(?:"
    r"TX\s+(?:legacy\s+GKH/XK76\s+registration|complete\s+registration|"
    r"controller\s+runtime\s+response)|"
    r"Controller\s+(?:registration|runtime\s+response)\s+TX\s+flush"
    r")"
)
OEM_TX_RE = re.compile(
    r"\bOEM\s+TX\s+(?P<label>\S+)\s+profile=(?P<profile>\S+)"
)
OEM_COMPLETE_RE = re.compile(
    r"\bOEM probe complete:\s+complete\s+valid=(?P<valid>\d+)\s+"
    r"last_cmd=0x(?P<last_cmd>[0-9A-Fa-f]{2})\s+"
    r"info44=(?P<info44>YES|NO)\s+status31=(?P<status31>YES|NO)"
)
BOOT_PROFILE_RE = re.compile(
    r"Passive UART profile scan enabled;.*\bboot_profile=(?P<profile>\S+)"
)
BOOT_STAGED_RE = re.compile(
    r"SCAN listening profile=(?P<profile>\S+) phase=boot-staged"
)
CLOCK_RE = re.compile(
    r"\[(?P<h>\d+):(?P<m>\d{2}):(?P<s>\d{2})\.(?P<f>\d{1,6})\]"
)

REFERENCE_BAUDS = (1200, 2400, 4800, 9600, 19200, 38400)


@dataclass(frozen=True)
class ProfileObservation:
    profile: str
    byte_count: int
    legacy_valid_frames: int
    source_line: int


@dataclass(frozen=True)
class HealthObservation:
    source_line: int
    timestamp_s: Optional[float]
    mode: str
    profile: str
    bytes_total: int
    uart_window: int
    valid_frames: int
    rx_edges_window: int
    rx_edges_total: int
    edge_cadence_samples: int
    edge_min_gap_us: int
    edge_max_gap_us: int
    edge_last_gap_us: int


@dataclass(frozen=True)
class BootProfileObservation:
    profile: str
    source_line: int
    source: str


@dataclass(frozen=True)
class CadenceHint:
    baud: int
    bit_period_us: float
    multiplier: int
    relative_error: float


@dataclass(frozen=True)
class OemProbeCompletion:
    source_line: int
    valid_frames: int
    last_command: int
    info44: bool
    status31: bool


@dataclass
class DiscoveryAnalysis:
    profiles: list[ProfileObservation]
    health: list[HealthObservation]
    transmit_lines: list[int]
    boot_profiles: list[BootProfileObservation]
    oem_transmit_lines: list[int]
    oem_probe_completion: Optional[OemProbeCompletion]

    @property
    def max_edges_total(self) -> int:
        return max((item.rx_edges_total for item in self.health), default=0)

    @property
    def max_bytes_total(self) -> int:
        return max((item.bytes_total for item in self.health), default=0)

    @property
    def max_valid_frames(self) -> int:
        return max((item.valid_frames for item in self.health), default=0)

    @property
    def profile_bytes_total(self) -> int:
        return sum(item.byte_count for item in self.profiles)

    @property
    def capture_duration_s(self) -> Optional[float]:
        timestamps = [
            item.timestamp_s for item in self.health if item.timestamp_s is not None
        ]
        if len(timestamps) < 2:
            return None

        # ESPHome's bracket clock is monotonic within one boot, but pasted
        # journals can contain more than one boot. Never let a clock reset turn
        # two short silent windows into one apparently long qualification run.
        longest = 0.0
        segment_start = timestamps[0]
        previous = timestamps[0]
        for current in timestamps[1:]:
            if current < previous:
                longest = max(longest, previous - segment_start)
                segment_start = current
            previous = current
        return max(longest, previous - segment_start)

    @property
    def declared_boot_profile(self) -> Optional[str]:
        values = {item.profile for item in self.boot_profiles}
        if len(values) == 1:
            return next(iter(values))
        return None

    @property
    def boot_profile_conflict(self) -> bool:
        return len({item.profile for item in self.boot_profiles}) > 1

    @property
    def min_edge_gap_us(self) -> int:
        values = [
            item.edge_min_gap_us
            for item in self.health
            if item.edge_cadence_samples > 0 and item.edge_min_gap_us > 0
        ]
        return min(values, default=0)


def _clock_to_seconds(line: str) -> Optional[float]:
    match = CLOCK_RE.search(line)
    if not match:
        return None
    fraction = match.group("f").ljust(6, "0")
    return (
        int(match.group("h")) * 3600
        + int(match.group("m")) * 60
        + int(match.group("s"))
        + int(fraction) / 1_000_000.0
    )


def _int_value(fields: dict[str, str], key: str) -> int:
    raw = fields.get(key, "0")
    try:
        return int(raw, 10)
    except ValueError:
        return 0


def analyze_log(text: str) -> DiscoveryAnalysis:
    profiles: list[ProfileObservation] = []
    health: list[HealthObservation] = []
    transmit_lines: list[int] = []
    boot_profiles: list[BootProfileObservation] = []
    oem_transmit_lines: list[int] = []
    oem_probe_completion: Optional[OemProbeCompletion] = None

    for line_number, line in enumerate(text.splitlines(), start=1):
        boot = BOOT_PROFILE_RE.search(line)
        if boot:
            boot_profiles.append(
                BootProfileObservation(
                    profile=boot.group("profile"),
                    source_line=line_number,
                    source="scan-enabled",
                )
            )

        staged = BOOT_STAGED_RE.search(line)
        if staged:
            boot_profiles.append(
                BootProfileObservation(
                    profile=staged.group("profile"),
                    source_line=line_number,
                    source="boot-staged",
                )
            )

        scan = SCAN_RE.search(line)
        if scan:
            profiles.append(
                ProfileObservation(
                    profile=scan.group("profile"),
                    byte_count=int(scan.group("bytes")),
                    legacy_valid_frames=int(scan.group("valid")),
                    source_line=line_number,
                )
            )

        health_match = HEALTH_RE.search(line)
        if health_match:
            fields = {
                match.group("key"): match.group("value")
                for match in KV_RE.finditer(health_match.group("body"))
            }
            health.append(
                HealthObservation(
                    source_line=line_number,
                    timestamp_s=_clock_to_seconds(line),
                    mode=fields.get("mode", "UNKNOWN"),
                    profile=fields.get("profile", "UNKNOWN"),
                    bytes_total=_int_value(fields, "bytes"),
                    uart_window=_int_value(fields, "uart_window"),
                    valid_frames=_int_value(fields, "valid"),
                    rx_edges_window=_int_value(fields, "rx_edges_window"),
                    rx_edges_total=_int_value(fields, "rx_edges_total"),
                    edge_cadence_samples=_int_value(fields, "edge_cadence_samples"),
                    edge_min_gap_us=_int_value(fields, "edge_min_gap_us"),
                    edge_max_gap_us=_int_value(fields, "edge_max_gap_us"),
                    edge_last_gap_us=_int_value(fields, "edge_last_gap_us"),
                )
            )

        if TX_RE.search(line):
            transmit_lines.append(line_number)

        oem_tx = OEM_TX_RE.search(line)
        if oem_tx:
            transmit_lines.append(line_number)
            oem_transmit_lines.append(line_number)

        oem_complete = OEM_COMPLETE_RE.search(line)
        if oem_complete:
            oem_probe_completion = OemProbeCompletion(
                source_line=line_number,
                valid_frames=int(oem_complete.group("valid")),
                last_command=int(oem_complete.group("last_cmd"), 16),
                info44=oem_complete.group("info44") == "YES",
                status31=oem_complete.group("status31") == "YES",
            )

    return DiscoveryAnalysis(
        profiles=profiles,
        health=health,
        transmit_lines=transmit_lines,
        boot_profiles=boot_profiles,
        oem_transmit_lines=oem_transmit_lines,
        oem_probe_completion=oem_probe_completion,
    )


def cadence_hints(min_gap_us: int, tolerance: float = 0.12) -> list[CadenceHint]:
    if min_gap_us <= 0:
        return []

    hints: list[CadenceHint] = []
    for baud in REFERENCE_BAUDS:
        bit_period = 1_000_000.0 / baud
        ratio = min_gap_us / bit_period
        multiplier = max(1, min(8, int(round(ratio))))
        expected = bit_period * multiplier
        error = abs(min_gap_us - expected) / expected
        if error <= tolerance:
            hints.append(
                CadenceHint(
                    baud=baud,
                    bit_period_us=bit_period,
                    multiplier=multiplier,
                    relative_error=error,
                )
            )

    hints.sort(key=lambda item: (item.relative_error, item.multiplier, item.baud))
    return hints


def sustained_physical_silence(
    analysis: DiscoveryAnalysis,
    min_duration_s: float = 60.0,
) -> bool:
    """True only for a long passive capture with no physical or UART activity.

    GPIO edge observation is below UART framing. Once a complete cold-start
    capture has zero receiver edges, changing baud/parity cannot reveal traffic
    that never toggled the receiver input.
    """
    if analysis.transmit_lines:
        return False
    duration = analysis.capture_duration_s
    if duration is None or duration < min_duration_s:
        return False
    return (
        analysis.max_edges_total == 0
        and analysis.max_bytes_total == 0
        and analysis.profile_bytes_total == 0
        and analysis.max_valid_frames == 0
    )


def recommended_next_step(analysis: DiscoveryAnalysis) -> str:
    state = conclusion(analysis)
    if state == "oem_rtl_probe_no_response":
        return "recover_direct_com_manual_protocol_from_onokom_gr3_firmware"
    if state == "oem_rtl_probe_response_present":
        return "analyze_oem_response_frames_and_promote_verified_transport"
    if state == "oem_rtl_probe_electrical_response_without_valid_frame":
        return "preserve_raw_response_and_test_only_evidence_backed_uart_variants"
    if state == "oem_rtl_probe_incomplete":
        return "capture_complete_bounded_oem_probe_run"
    if analysis.transmit_lines:
        return "repeat_as_receive_only_capture"
    if sustained_physical_silence(analysis):
        return "recover_direct_com_manual_protocol_from_onokom_gr3_firmware"
    if state == "edge_activity_without_uart_decode":
        return "run_cold_start_serial_profile_matrix"
    if state == "uart_decode_candidates_without_legacy_validation":
        return "preserve_raw_bytes_and_compare_candidate_profiles"
    if state == "legacy_frame_evidence_present":
        return "analyze_complete_frames_and_timing"
    if state == "electrically_silent":
        return "extend_capture_to_60s_before_concluding_physical_silence"
    return "capture_more_evidence"


def conclusion(analysis: DiscoveryAnalysis) -> str:
    if analysis.oem_probe_completion is not None:
        completion = analysis.oem_probe_completion
        if completion.valid_frames > 0:
            return "oem_rtl_probe_response_present"
        if analysis.max_edges_total == 0 and analysis.max_bytes_total == 0:
            return "oem_rtl_probe_no_response"
        return "oem_rtl_probe_electrical_response_without_valid_frame"
    if analysis.oem_transmit_lines:
        return "oem_rtl_probe_incomplete"
    if analysis.transmit_lines:
        return "capture_contains_tx_evidence"
    if not analysis.health and not analysis.profiles:
        return "insufficient_evidence"
    if any(item.legacy_valid_frames > 0 for item in analysis.profiles) or analysis.max_valid_frames > 0:
        return "legacy_frame_evidence_present"
    uart_evidence = analysis.max_bytes_total > 0 or analysis.profile_bytes_total > 0
    if analysis.max_edges_total == 0 and not uart_evidence:
        return "electrically_silent"
    if analysis.max_edges_total > 0 and not uart_evidence:
        return "edge_activity_without_uart_decode"
    if uart_evidence:
        return "uart_decode_candidates_without_legacy_validation"
    return "insufficient_evidence"


def profile_summary(analysis: DiscoveryAnalysis) -> list[dict]:
    grouped: dict[str, dict[str, int]] = {}
    for item in analysis.profiles:
        current = grouped.setdefault(
            item.profile,
            {"windows": 0, "bytes": 0, "legacy_valid_frames": 0},
        )
        current["windows"] += 1
        current["bytes"] += item.byte_count
        current["legacy_valid_frames"] += item.legacy_valid_frames

    return [
        {"profile": profile, **values}
        for profile, values in sorted(
            grouped.items(),
            key=lambda pair: (
                -pair[1]["legacy_valid_frames"],
                -pair[1]["bytes"],
                pair[0],
            ),
        )
    ]


def summary_dict(analysis: DiscoveryAnalysis) -> dict:
    modes = Counter(item.mode for item in analysis.health)
    return {
        "conclusion": conclusion(analysis),
        "transmit_lines": analysis.transmit_lines,
        "oem_transmit_lines": analysis.oem_transmit_lines,
        "oem_probe_completion": (
            asdict(analysis.oem_probe_completion)
            if analysis.oem_probe_completion is not None
            else None
        ),
        "boot_profiles": [asdict(item) for item in analysis.boot_profiles],
        "declared_boot_profile": analysis.declared_boot_profile,
        "boot_profile_conflict": analysis.boot_profile_conflict,
        "capture_duration_s": analysis.capture_duration_s,
        "sustained_physical_silence": sustained_physical_silence(analysis),
        "recommended_next_step": recommended_next_step(analysis),
        "health_modes": dict(sorted(modes.items())),
        "max_edges_total": analysis.max_edges_total,
        "max_bytes_total": analysis.max_bytes_total,
        "max_valid_frames": analysis.max_valid_frames,
        "profile_bytes_total": analysis.profile_bytes_total,
        "min_edge_gap_us": analysis.min_edge_gap_us,
        "cadence_hints": [asdict(item) for item in cadence_hints(analysis.min_edge_gap_us)],
        "profiles": profile_summary(analysis),
    }


def _print_human(analysis: DiscoveryAnalysis) -> None:
    summary = summary_dict(analysis)
    print(f"conclusion={summary['conclusion']}")
    print(f"next_step={summary['recommended_next_step']}")
    print(
        "capture "
        f"boot_profile={summary['declared_boot_profile'] or 'UNKNOWN'} "
        f"boot_profile_conflict={summary['boot_profile_conflict']} "
        f"duration_s={summary['capture_duration_s'] if summary['capture_duration_s'] is not None else '-'}"
    )
    print(
        "evidence "
        f"max_edges_total={summary['max_edges_total']} "
        f"max_bytes_total={summary['max_bytes_total']} "
        f"max_valid_frames={summary['max_valid_frames']} "
        f"profile_bytes_total={summary['profile_bytes_total']} "
        f"min_edge_gap_us={summary['min_edge_gap_us']}"
    )

    if summary["transmit_lines"]:
        print(
            "warning: transmit evidence on source line(s): "
            + ", ".join(str(value) for value in summary["transmit_lines"])
        )

    if summary["profiles"]:
        print("profiles:")
        for item in summary["profiles"]:
            print(
                "  "
                f"{item['profile']}: windows={item['windows']} "
                f"bytes={item['bytes']} "
                f"legacy_valid={item['legacy_valid_frames']}"
            )

    hints = summary["cadence_hints"]
    if hints:
        print("edge cadence hints (not protocol identification):")
        for item in hints:
            print(
                "  "
                f"{item['baud']} baud x{item['multiplier']} "
                f"bit={item['bit_period_us']:.1f}us "
                f"error={item['relative_error'] * 100.0:.1f}%"
            )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Summarize passive XE71/XE72 COM-MANUAL discovery logs"
    )
    parser.add_argument("capture", type=Path, help="ESPHome text log")
    parser.add_argument("--json", action="store_true", help="emit JSON")
    args = parser.parse_args()

    analysis = analyze_log(args.capture.read_text(errors="replace"))
    if args.json:
        print(json.dumps(summary_dict(analysis), indent=2, sort_keys=True))
    else:
        _print_human(analysis)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
