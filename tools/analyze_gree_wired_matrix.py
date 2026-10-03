#!/usr/bin/env python3
"""Compare one-breaker-cycle-per-profile XE71/XE72 discovery captures."""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

import analyze_gree_wired_discovery as discovery


EXPECTED_PROFILES = (
    "1200-8N1", "1200-8E1",
    "2400-8N1", "2400-8E1",
    "4800-8N1", "4800-8E1",
    "9600-8N1", "9600-8E1",
    "19200-8N1", "19200-8E1",
    "38400-8N1", "38400-8E1",
)


@dataclass(frozen=True)
class CaptureRow:
    path: str
    boot_profile: Optional[str]
    boot_profile_conflict: bool
    conclusion: str
    transmit_lines: tuple[int, ...]
    duration_s: Optional[float]
    max_edges_total: int
    max_bytes_total: int
    max_valid_frames: int
    profile_bytes_total: int
    min_edge_gap_us: int

    def to_dict(self) -> dict:
        return {
            "path": self.path,
            "boot_profile": self.boot_profile,
            "boot_profile_conflict": self.boot_profile_conflict,
            "conclusion": self.conclusion,
            "transmit_lines": list(self.transmit_lines),
            "duration_s": self.duration_s,
            "max_edges_total": self.max_edges_total,
            "max_bytes_total": self.max_bytes_total,
            "max_valid_frames": self.max_valid_frames,
            "profile_bytes_total": self.profile_bytes_total,
            "min_edge_gap_us": self.min_edge_gap_us,
        }


def analyze_paths(paths: list[Path]) -> list[CaptureRow]:
    rows: list[CaptureRow] = []
    for path in paths:
        analysis = discovery.analyze_log(path.read_text(errors="replace"))
        rows.append(
            CaptureRow(
                path=str(path),
                boot_profile=analysis.declared_boot_profile,
                boot_profile_conflict=analysis.boot_profile_conflict,
                conclusion=discovery.conclusion(analysis),
                transmit_lines=tuple(analysis.transmit_lines),
                duration_s=analysis.capture_duration_s,
                max_edges_total=analysis.max_edges_total,
                max_bytes_total=analysis.max_bytes_total,
                max_valid_frames=analysis.max_valid_frames,
                profile_bytes_total=analysis.profile_bytes_total,
                min_edge_gap_us=analysis.min_edge_gap_us,
            )
        )
    return rows


def coverage(rows: list[CaptureRow]) -> dict:
    observed = [row.boot_profile for row in rows if row.boot_profile is not None]
    counts = {profile: observed.count(profile) for profile in EXPECTED_PROFILES}
    missing = [profile for profile, count in counts.items() if count == 0]
    duplicates = [profile for profile, count in counts.items() if count > 1]
    unknown = [row.path for row in rows if row.boot_profile is None]
    conflicts = [row.path for row in rows if row.boot_profile_conflict]
    tx = [row.path for row in rows if row.transmit_lines]
    short = [
        row.path
        for row in rows
        if row.duration_s is not None and row.duration_s < 60.0
    ]
    return {
        "expected_profiles": list(EXPECTED_PROFILES),
        "observed_profiles": observed,
        "missing_profiles": missing,
        "duplicate_profiles": duplicates,
        "unknown_profile_captures": unknown,
        "conflicting_profile_captures": conflicts,
        "captures_with_tx": tx,
        "captures_under_60s": short,
        "complete_passive_matrix": not (missing or duplicates or unknown or conflicts or tx),
    }


def summary(rows: list[CaptureRow]) -> dict:
    return {
        "coverage": coverage(rows),
        "captures": [row.to_dict() for row in rows],
    }


def print_human(rows: list[CaptureRow]) -> None:
    cov = coverage(rows)
    print(
        "matrix "
        f"complete_passive={cov['complete_passive_matrix']} "
        f"captures={len(rows)} missing={len(cov['missing_profiles'])} "
        f"duplicates={len(cov['duplicate_profiles'])} "
        f"tx={len(cov['captures_with_tx'])}"
    )
    for row in rows:
        duration = "-" if row.duration_s is None else f"{row.duration_s:.1f}"
        print(
            f"{row.boot_profile or 'UNKNOWN':>10} "
            f"duration={duration:>6}s "
            f"conclusion={row.conclusion} "
            f"edges={row.max_edges_total} bytes={row.max_bytes_total} "
            f"valid={row.max_valid_frames} file={row.path}"
        )
    if cov["missing_profiles"]:
        print("missing:", ", ".join(cov["missing_profiles"]))
    if cov["duplicate_profiles"]:
        print("duplicates:", ", ".join(cov["duplicate_profiles"]))
    if cov["unknown_profile_captures"]:
        print("unknown-profile:", ", ".join(cov["unknown_profile_captures"]))
    if cov["conflicting_profile_captures"]:
        print("profile-conflict:", ", ".join(cov["conflicting_profile_captures"]))
    if cov["captures_with_tx"]:
        print("TX-EVIDENCE:", ", ".join(cov["captures_with_tx"]))
    if cov["captures_under_60s"]:
        print("under-60s:", ", ".join(cov["captures_under_60s"]))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare passive XE71/XE72 cold-start serial-profile captures"
    )
    parser.add_argument("captures", nargs="+", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    rows = analyze_paths(args.captures)
    if args.json:
        print(json.dumps(summary(rows), indent=2, sort_keys=True))
    else:
        print_human(rows)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
