#!/usr/bin/env python3
"""Analyze Gree COM-MANUAL wired-controller captures.

The classifier names are provenance labels, not claims that a Vireo/XE71 uses
the same application-layer messages.  It recognizes the public XK19 and
GKH/XK76 reference layouts so an authentic XE71/XE72 capture can be compared
without hand-decoding frames.
"""

from __future__ import annotations

import argparse
import csv
import io
import json
import re
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, Optional


HEX_BYTE_RE = re.compile(r"(?<![0-9A-Fa-f])([0-9A-Fa-f]{2})(?![0-9A-Fa-f])")
FRAME_MARKER_RE = re.compile(r"(?i)(?<![0-9A-Fa-f])7E\s+7E(?![0-9A-Fa-f])")
CLOCK_RE = re.compile(
    r"\[(?P<h>\d{2}):(?P<m>\d{2}):(?P<s>\d{2})\.(?P<f>\d{1,6})\]"
)

REFERENCE_LAYOUTS = {
    (0x00, 0xFF, 0x0E): "legacy_poll_00_ff_0e",
    (0xFF, 0x00, 0x15): "xk19_controller_state_ff00_15",
    (0xFF, 0x00, 0x22): "gkh_xk76_controller_state_ff00_22",
    (0xFF, 0x40, 0x16): "xk19_status_ff40_16",
    (0xFF, 0x40, 0x17): "gkh_pre_registration_status_ff40_17",
    (0xFF, 0x40, 0x29): "gkh_registered_status_ff40_29",
}


@dataclass(frozen=True)
class FrameRecord:
    timestamp_s: Optional[float]
    source_line: Optional[int]
    direction: str
    raw_hex: str
    source: int
    destination: int
    message_type: int
    body_length: int
    total_length: int
    xor_valid: bool
    route: str
    reference_layout: str
    signature: Optional[str]

    def to_json(self) -> dict:
        data = asdict(self)
        data["source"] = f"{self.source:02X}"
        data["destination"] = f"{self.destination:02X}"
        data["message_type"] = f"{self.message_type:02X}"
        data["body_length"] = f"{self.body_length:02X}"
        return data


@dataclass
class TraceAnalysis:
    frames: list[FrameRecord]
    truncated_candidates: int = 0

    @property
    def valid_xor_frames(self) -> int:
        return sum(frame.xor_valid for frame in self.frames)


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


def _signature_for(source: int, destination: int, payload: list[int]) -> Optional[str]:
    if source != 0xFF or destination not in (0x00, 0x40) or len(payload) < 3:
        return None
    return " ".join(f"{value:02X}" for value in payload[:3])


# Byte positions 2 and 3 are retained as source/destination for compatibility
# with the original public reverse-engineering terminology. Field captures show
# that the 00/FF pair cannot yet be trusted as literal physical node addresses:
# an indoor unit with no wired controller attached emits both FF 40 and 00 FF
# frames. Treat route strings such as 00->FF as structural byte-pair labels.
def decode_frame(
    raw: Iterable[int],
    *,
    timestamp_s: Optional[float] = None,
    source_line: Optional[int] = None,
    direction: str = "observed",
) -> FrameRecord:
    data = [int(value) & 0xFF for value in raw]
    if len(data) < 7:
        raise ValueError("frame is shorter than the 6-byte header plus checksum")
    if data[0:2] != [0x7E, 0x7E]:
        raise ValueError("frame does not begin with 7E 7E")

    body_length = data[5]
    expected_length = 6 + body_length
    if len(data) != expected_length:
        raise ValueError(
            f"declared body length 0x{body_length:02X} requires "
            f"{expected_length} bytes, got {len(data)}"
        )

    checksum = 0
    for value in data:
        checksum ^= value

    source = data[2]
    destination = data[3]
    message_type = data[4]
    payload = data[6:-1]
    reference_layout = REFERENCE_LAYOUTS.get(
        (source, destination, body_length),
        "unknown",
    )
    return FrameRecord(
        timestamp_s=timestamp_s,
        source_line=source_line,
        direction=direction,
        raw_hex=" ".join(f"{value:02X}" for value in data),
        source=source,
        destination=destination,
        message_type=message_type,
        body_length=body_length,
        total_length=len(data),
        xor_valid=checksum == 0,
        route=f"{source:02X}->{destination:02X}",
        reference_layout=reference_layout,
        signature=_signature_for(source, destination, payload),
    )


def _scan_sequence(
    values: list[int],
    *,
    timestamps: Optional[list[Optional[float]]] = None,
    source_line: Optional[int] = None,
    direction: str = "observed",
) -> TraceAnalysis:
    frames: list[FrameRecord] = []
    truncated = 0
    index = 0

    while index + 1 < len(values):
        if values[index:index + 2] != [0x7E, 0x7E]:
            index += 1
            continue
        if index + 6 > len(values):
            truncated += 1
            break

        body_length = values[index + 5]
        total_length = 6 + body_length
        end = index + total_length
        if end > len(values):
            truncated += 1
            break

        timestamp_s = None
        if timestamps is not None and index < len(timestamps):
            timestamp_s = timestamps[index]

        frame = decode_frame(
            values[index:end],
            timestamp_s=timestamp_s,
            source_line=source_line,
            direction=direction,
        )
        frames.append(frame)
        index = end

    return TraceAnalysis(frames=frames, truncated_candidates=truncated)


def _direction_for_text_line(line: str, marker_start: int) -> str:
    prefix = line[:marker_start].upper()
    if "TX " in prefix or "TX:" in prefix:
        return "tx"
    if (
        "RX " in prefix
        or "RX:" in prefix
        or "UART_DEBUG" in prefix
        or "[COM-MANUAL]" in prefix
    ):
        return "rx"
    return "observed"


def analyze_text(text: str) -> TraceAnalysis:
    frames: list[FrameRecord] = []
    truncated = 0

    for line_number, line in enumerate(text.splitlines(), start=1):
        marker = FRAME_MARKER_RE.search(line)
        if not marker:
            continue
        tail = line[marker.start():]
        values = [int(token, 16) for token in HEX_BYTE_RE.findall(tail)]
        result = _scan_sequence(
            values,
            timestamps=[_clock_to_seconds(line)] * len(values),
            source_line=line_number,
            direction=_direction_for_text_line(line, marker.start()),
        )
        frames.extend(result.frames)
        truncated += result.truncated_candidates

    return TraceAnalysis(frames=frames, truncated_candidates=truncated)


def _pick_column(fieldnames: list[str], needles: tuple[str, ...]) -> Optional[str]:
    for field in fieldnames:
        lowered = field.lower()
        if any(needle in lowered for needle in needles):
            return field
    return None


def _parse_csv_byte(value: str) -> Optional[int]:
    stripped = value.strip()
    match = re.fullmatch(r"(?i)(?:0x)?([0-9a-f]{2})", stripped)
    return int(match.group(1), 16) if match else None


def analyze_saleae_csv(text: str) -> TraceAnalysis:
    reader = csv.DictReader(io.StringIO(text))
    if not reader.fieldnames:
        return TraceAnalysis([])

    time_column = _pick_column(reader.fieldnames, ("time",))
    value_column = _pick_column(reader.fieldnames, ("value", "data", "byte"))
    if value_column is None:
        raise ValueError("CSV has no Value/Data/Byte column")

    values: list[int] = []
    timestamps: list[Optional[float]] = []
    for row in reader:
        value = _parse_csv_byte(row.get(value_column, ""))
        if value is None:
            continue

        timestamp_s: Optional[float] = None
        if time_column is not None:
            raw_time = row.get(time_column, "").strip()
            if raw_time:
                try:
                    timestamp_s = float(raw_time)
                except ValueError:
                    pass

        values.append(value)
        timestamps.append(timestamp_s)

    return _scan_sequence(values, timestamps=timestamps)


def analyze_capture(text: str) -> TraceAnalysis:
    first_line = text.splitlines()[0] if text.splitlines() else ""
    if "," in first_line:
        try:
            csv_result = analyze_saleae_csv(text)
            if csv_result.frames:
                return csv_result
        except ValueError:
            pass
    return analyze_text(text)


def session_summary(analysis: TraceAnalysis) -> dict:
    """Describe startup ordering without promoting legacy frames to XE71 semantics."""
    frames = [frame for frame in analysis.frames if frame.xor_valid]

    controller_indices = [
        index
        for index, frame in enumerate(frames)
        if frame.route == "FF->00"
    ]
    first_controller_index = (
        controller_indices[0] if controller_indices else None
    )

    poll_indices = [
        index
        for index, frame in enumerate(frames)
        if frame.reference_layout == "legacy_poll_00_ff_0e"
    ]
    pre_status_indices = [
        index
        for index, frame in enumerate(frames)
        if frame.reference_layout in (
            "xk19_status_ff40_16",
            "gkh_pre_registration_status_ff40_17",
        )
    ]
    expanded_status_indices = [
        index
        for index, frame in enumerate(frames)
        if frame.reference_layout == "gkh_registered_status_ff40_29"
    ]

    before_controller = (
        first_controller_index
        if first_controller_index is not None
        else len(frames)
    )
    polls_before_controller = sum(
        index < before_controller for index in poll_indices
    )
    statuses_before_controller = sum(
        index < before_controller for index in pre_status_indices
    )
    expansion_after_controller = (
        first_controller_index is not None
        and any(index > first_controller_index for index in expanded_status_indices)
    )

    if (
        first_controller_index is not None
        and (polls_before_controller > 0 or statuses_before_controller > 0)
        and expansion_after_controller
    ):
        pattern = "legacy_indoor_first_then_controller_registration"
    elif (
        first_controller_index is not None
        and (polls_before_controller > 0 or statuses_before_controller > 0)
    ):
        pattern = "legacy_indoor_first_then_controller_reply"
    elif (
        first_controller_index is None
        and (poll_indices or pre_status_indices)
    ):
        pattern = "legacy_indoor_first_no_controller_reply"
    elif first_controller_index is not None:
        pattern = "controller_frame_without_observed_indoor_startup"
    elif frames:
        pattern = "nonlegacy_or_partial_wired_capture"
    else:
        pattern = "no_valid_wired_frames"

    first_timestamp = next(
        (frame.timestamp_s for frame in frames if frame.timestamp_s is not None),
        None,
    )
    first_controller_timestamp = (
        frames[first_controller_index].timestamp_s
        if first_controller_index is not None
        else None
    )
    first_expanded_timestamp = (
        frames[expanded_status_indices[0]].timestamp_s
        if expanded_status_indices
        else None
    )

    def delay_from_first(value: Optional[float]) -> Optional[float]:
        if value is None or first_timestamp is None:
            return None
        return value - first_timestamp

    return {
        "pattern": pattern,
        "valid_frames_considered": len(frames),
        "legacy_polls": len(poll_indices),
        "pre_registration_status_frames": len(pre_status_indices),
        "controller_state_frames": len(controller_indices),
        "expanded_status_frames": len(expanded_status_indices),
        "polls_before_first_controller": polls_before_controller,
        "statuses_before_first_controller": statuses_before_controller,
        "expanded_status_after_controller": expansion_after_controller,
        "first_controller_delay_s": delay_from_first(first_controller_timestamp),
        "first_expanded_status_delay_s": delay_from_first(first_expanded_timestamp),
    }


def _summary_dict(analysis: TraceAnalysis) -> dict:
    route_counts = Counter(frame.route for frame in analysis.frames)
    layout_counts = Counter(frame.reference_layout for frame in analysis.frames)
    direction_counts = Counter(frame.direction for frame in analysis.frames)
    signatures = Counter(
        frame.signature for frame in analysis.frames if frame.signature is not None
    )
    return {
        "frames": len(analysis.frames),
        "valid_xor_frames": analysis.valid_xor_frames,
        "invalid_xor_frames": len(analysis.frames) - analysis.valid_xor_frames,
        "truncated_candidates": analysis.truncated_candidates,
        "routes": dict(sorted(route_counts.items())),
        "reference_layouts": dict(sorted(layout_counts.items())),
        "directions": dict(sorted(direction_counts.items())),
        "signatures": dict(sorted(signatures.items())),
        "session": session_summary(analysis),
    }


def _print_human(analysis: TraceAnalysis) -> None:
    summary = _summary_dict(analysis)
    print(
        "frames={frames} xor_ok={valid_xor_frames} xor_bad={invalid_xor_frames} "
        "truncated={truncated_candidates}".format(**summary)
    )
    previous_timestamp: Optional[float] = None
    for index, frame in enumerate(analysis.frames, start=1):
        timestamp = "-" if frame.timestamp_s is None else f"{frame.timestamp_s:.6f}s"
        delta = "-"
        if frame.timestamp_s is not None and previous_timestamp is not None:
            delta = f"{frame.timestamp_s - previous_timestamp:.6f}s"
        if frame.timestamp_s is not None:
            previous_timestamp = frame.timestamp_s
        signature = frame.signature or "-"
        print(
            f"{index:04d} t={timestamp} dt={delta} dir={frame.direction} "
            f"route={frame.route} type=0x{frame.message_type:02X} "
            f"len=0x{frame.body_length:02X} "
            f"xor={'OK' if frame.xor_valid else 'BAD'} "
            f"layout={frame.reference_layout} sig={signature}"
        )
        print(f"     {frame.raw_hex}")

    if summary["routes"]:
        print("routes:", json.dumps(summary["routes"], sort_keys=True))
    if summary["reference_layouts"]:
        print(
            "reference_layouts:",
            json.dumps(summary["reference_layouts"], sort_keys=True),
        )
    if summary["directions"]:
        print("directions:", json.dumps(summary["directions"], sort_keys=True))
    if summary["signatures"]:
        print("signatures:", json.dumps(summary["signatures"], sort_keys=True))
    session = summary["session"]
    print(
        "session: "
        f"pattern={session['pattern']} "
        f"polls={session['legacy_polls']} "
        f"pre_status={session['pre_registration_status_frames']} "
        f"controller={session['controller_state_frames']} "
        f"expanded={session['expanded_status_frames']} "
        f"polls_before_controller={session['polls_before_first_controller']} "
        f"expanded_after_controller={session['expanded_status_after_controller']}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Analyze Gree COM-MANUAL 7E 7E wired-controller captures"
    )
    parser.add_argument("capture", type=Path, help="text log or Saleae async-serial CSV")
    parser.add_argument("--json", action="store_true", help="emit JSON")
    parser.add_argument(
        "--exclude-tx",
        action="store_true",
        help="exclude frames explicitly logged as local TX",
    )
    args = parser.parse_args()

    analysis = analyze_capture(args.capture.read_text(errors="replace"))
    if args.exclude_tx:
        analysis = TraceAnalysis(
            frames=[frame for frame in analysis.frames if frame.direction != "tx"],
            truncated_candidates=analysis.truncated_candidates,
        )
    if args.json:
        print(
            json.dumps(
                {
                    "summary": _summary_dict(analysis),
                    "frames": [frame.to_json() for frame in analysis.frames],
                },
                indent=2,
                sort_keys=True,
            )
        )
    else:
        _print_human(analysis)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
