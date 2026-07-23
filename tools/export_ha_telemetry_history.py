#!/usr/bin/env python3
"""Export recorder history for ESPHome Gree UART telemetry entities.

The tool is read-only. It downloads state history from Home Assistant's REST API,
keeps the original JSON, writes a flat state-change CSV, expands hexadecimal UART
payloads into byte columns, and produces per-byte change statistics.
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import os
import re
import ssl
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any, Iterable

HEX_BYTE_RE = re.compile(r"(?i)(?<![0-9a-f])([0-9a-f]{2})(?![0-9a-f])")
COMMAND_RE = re.compile(r"(?i)(?:^|[; ,])(?:cmd|command)=0x?([0-9a-f]{2})(?:$|[; ,])")
PAYLOAD_MARKERS = ("payload=", " p=")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Export and decode Home Assistant recorder history for Gree UART telemetry."
    )
    parser.add_argument(
        "--ha-url",
        default=os.environ.get("HA_URL"),
        help="Home Assistant base URL, for example http://homeassistant.local:8123 (or HA_URL).",
    )
    parser.add_argument(
        "--token",
        default=os.environ.get("HA_TOKEN"),
        help="Home Assistant long-lived access token (or HA_TOKEN).",
    )
    parser.add_argument(
        "--entity",
        action="append",
        default=[],
        help="Entity ID to export. Repeat for multiple entities.",
    )
    parser.add_argument(
        "--match",
        action="append",
        default=[],
        help="Case-insensitive substring used to select entity IDs from /api/states. Repeatable.",
    )
    parser.add_argument(
        "--hours",
        type=float,
        default=6.0,
        help="Hours of history to retrieve when --start is not supplied (default: 6).",
    )
    parser.add_argument(
        "--start",
        help="ISO-8601 start timestamp. Overrides --hours.",
    )
    parser.add_argument(
        "--end",
        help="ISO-8601 end timestamp. Defaults to now.",
    )
    parser.add_argument(
        "--output-prefix",
        default=None,
        help="Output path prefix. Default: gree-telemetry-<UTC timestamp>.",
    )
    parser.add_argument(
        "--insecure",
        action="store_true",
        help="Disable TLS certificate verification for a trusted local HA endpoint.",
    )
    return parser.parse_args()


def utc_now() -> dt.datetime:
    return dt.datetime.now(dt.timezone.utc)


def parse_iso(value: str) -> dt.datetime:
    normalized = value.strip().replace("Z", "+00:00")
    parsed = dt.datetime.fromisoformat(normalized)
    if parsed.tzinfo is None:
        parsed = parsed.replace(tzinfo=dt.timezone.utc)
    return parsed.astimezone(dt.timezone.utc)


def iso_utc(value: dt.datetime) -> str:
    return value.astimezone(dt.timezone.utc).isoformat(timespec="seconds")


def api_get(base_url: str, token: str, path: str, insecure: bool) -> Any:
    url = f"{base_url.rstrip('/')}{path}"
    request = urllib.request.Request(
        url,
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
            "Accept": "application/json",
        },
    )
    context = ssl._create_unverified_context() if insecure else None
    try:
        with urllib.request.urlopen(request, timeout=60, context=context) as response:
            payload = response.read().decode("utf-8")
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"Home Assistant returned HTTP {exc.code}: {detail}") from exc
    except urllib.error.URLError as exc:
        raise RuntimeError(f"Unable to reach Home Assistant: {exc.reason}") from exc
    return json.loads(payload)


def resolve_entities(
    base_url: str,
    token: str,
    explicit: Iterable[str],
    matches: Iterable[str],
    insecure: bool,
) -> list[str]:
    entities = {item.strip() for item in explicit if item.strip()}
    needles = [item.lower().strip() for item in matches if item.strip()]
    if needles:
        states = api_get(base_url, token, "/api/states", insecure)
        for state in states:
            entity_id = str(state.get("entity_id", ""))
            friendly_name = str(state.get("attributes", {}).get("friendly_name", ""))
            haystack = f"{entity_id} {friendly_name}".lower()
            if any(needle in haystack for needle in needles):
                entities.add(entity_id)
    return sorted(entities)


def fetch_history(
    base_url: str,
    token: str,
    entities: list[str],
    start: dt.datetime,
    end: dt.datetime,
    insecure: bool,
) -> list[list[dict[str, Any]]]:
    encoded_start = urllib.parse.quote(iso_utc(start), safe="")
    query = urllib.parse.urlencode(
        {
            "end_time": iso_utc(end),
            "filter_entity_id": ",".join(entities),
            "minimal_response": "",
            "no_attributes": "",
        }
    )
    path = f"/api/history/period/{encoded_start}?{query}"
    return api_get(base_url, token, path, insecure)


def flatten_history(history: list[list[dict[str, Any]]]) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for entity_history in history:
        for item in entity_history:
            rows.append(
                {
                    "entity_id": str(item.get("entity_id", "")),
                    "last_changed": str(item.get("last_changed", "")),
                    "last_updated": str(item.get("last_updated", item.get("last_changed", ""))),
                    "state": str(item.get("state", "")),
                }
            )
    rows.sort(key=lambda row: (row["last_changed"], row["entity_id"]))
    return rows


def extract_payload_text(state: str) -> str:
    lower = state.lower()
    for marker in PAYLOAD_MARKERS:
        index = lower.find(marker)
        if index >= 0:
            return state[index + len(marker) :]
    return state


def parse_payload(state: str) -> list[int]:
    payload_text = extract_payload_text(state)
    matches = HEX_BYTE_RE.findall(payload_text)
    # A real report is at least a few bytes. This avoids interpreting ordinary
    # numeric/text states as UART payloads.
    if len(matches) < 3:
        return []
    return [int(value, 16) for value in matches]


def infer_command(entity_id: str, state: str) -> str:
    match = COMMAND_RE.search(state)
    if match:
        return match.group(1).upper()
    entity_lower = entity_id.lower()
    for command in ("31", "33", "40", "44"):
        if f"0x{command}" in entity_lower or f"_{command}_" in entity_lower:
            return command
    return ""


def payload_rows(state_rows: list[dict[str, str]]) -> tuple[list[dict[str, Any]], int]:
    decoded: list[dict[str, Any]] = []
    max_bytes = 0
    previous: dict[tuple[str, str], list[int]] = {}
    for row in state_rows:
        payload = parse_payload(row["state"])
        if not payload:
            continue
        command = infer_command(row["entity_id"], row["state"])
        key = (row["entity_id"], command)
        prior = previous.get(key)
        if prior is None:
            changed_indexes = list(range(len(payload)))
        else:
            changed_indexes = [
                index
                for index in range(max(len(prior), len(payload)))
                if index >= len(prior) or index >= len(payload) or prior[index] != payload[index]
            ]
        previous[key] = payload
        max_bytes = max(max_bytes, len(payload))
        decoded.append(
            {
                "entity_id": row["entity_id"],
                "last_changed": row["last_changed"],
                "command": command,
                "byte_count": len(payload),
                "changed_indexes": " ".join(str(index) for index in changed_indexes),
                "payload": payload,
            }
        )
    return decoded, max_bytes


def calculate_byte_stats(decoded_rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    stats: dict[tuple[str, str, int], dict[str, Any]] = {}
    previous: dict[tuple[str, str], list[int]] = {}
    for row in decoded_rows:
        entity_id = row["entity_id"]
        command = row["command"]
        payload: list[int] = row["payload"]
        stream_key = (entity_id, command)
        prior = previous.get(stream_key)
        for index, value in enumerate(payload):
            key = (entity_id, command, index)
            item = stats.setdefault(
                key,
                {
                    "entity_id": entity_id,
                    "command": command,
                    "byte_index": index,
                    "samples": 0,
                    "changes": 0,
                    "minimum": value,
                    "maximum": value,
                    "latest": value,
                    "changed_bits": 0,
                    "first_changed": "",
                    "last_changed": "",
                },
            )
            item["samples"] += 1
            item["minimum"] = min(item["minimum"], value)
            item["maximum"] = max(item["maximum"], value)
            if prior is not None and index < len(prior) and prior[index] != value:
                item["changes"] += 1
                item["changed_bits"] |= prior[index] ^ value
                if not item["first_changed"]:
                    item["first_changed"] = row["last_changed"]
                item["last_changed"] = row["last_changed"]
            item["latest"] = value
        previous[stream_key] = payload
    result = list(stats.values())
    result.sort(key=lambda item: (item["entity_id"], item["command"], item["byte_index"]))
    return result


def write_json(path: Path, value: Any) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True), encoding="utf-8")


def write_states_csv(path: Path, rows: list[dict[str, str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["entity_id", "last_changed", "last_updated", "state"],
        )
        writer.writeheader()
        writer.writerows(rows)


def write_payload_csv(path: Path, rows: list[dict[str, Any]], max_bytes: int) -> None:
    fields = ["entity_id", "last_changed", "command", "byte_count", "changed_indexes"] + [
        f"b{index:03d}" for index in range(max_bytes)
    ]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            output = {key: row[key] for key in fields[:5]}
            for index, value in enumerate(row["payload"]):
                output[f"b{index:03d}"] = f"0x{value:02X}"
            writer.writerow(output)


def write_stats_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    fields = [
        "entity_id",
        "command",
        "byte_index",
        "samples",
        "changes",
        "minimum",
        "maximum",
        "latest",
        "changed_bits",
        "first_changed",
        "last_changed",
    ]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            output = dict(row)
            for key in ("minimum", "maximum", "latest", "changed_bits"):
                output[key] = f"0x{int(output[key]):02X}"
            writer.writerow(output)


def main() -> int:
    args = parse_args()
    if not args.ha_url:
        print("error: --ha-url or HA_URL is required", file=sys.stderr)
        return 2
    if not args.token:
        print("error: --token or HA_TOKEN is required", file=sys.stderr)
        return 2
    if args.hours <= 0:
        print("error: --hours must be greater than zero", file=sys.stderr)
        return 2

    end = parse_iso(args.end) if args.end else utc_now()
    start = parse_iso(args.start) if args.start else end - dt.timedelta(hours=args.hours)
    if start >= end:
        print("error: start must be earlier than end", file=sys.stderr)
        return 2

    try:
        entities = resolve_entities(
            args.ha_url, args.token, args.entity, args.match, args.insecure
        )
        if not entities:
            print(
                "error: no entities selected; use --entity or --match",
                file=sys.stderr,
            )
            return 2
        history = fetch_history(
            args.ha_url, args.token, entities, start, end, args.insecure
        )
    except (RuntimeError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    prefix = Path(
        args.output_prefix
        or f"gree-telemetry-{utc_now().strftime('%Y%m%dT%H%M%SZ')}"
    )
    prefix.parent.mkdir(parents=True, exist_ok=True)

    state_rows = flatten_history(history)
    decoded_rows, max_bytes = payload_rows(state_rows)
    stats = calculate_byte_stats(decoded_rows)

    json_path = prefix.with_suffix(".json")
    states_path = prefix.with_name(prefix.name + "-states.csv")
    payload_path = prefix.with_name(prefix.name + "-payloads.csv")
    stats_path = prefix.with_name(prefix.name + "-byte-stats.csv")

    write_json(
        json_path,
        {
            "start": iso_utc(start),
            "end": iso_utc(end),
            "entities": entities,
            "history": history,
        },
    )
    write_states_csv(states_path, state_rows)
    write_payload_csv(payload_path, decoded_rows, max_bytes)
    write_stats_csv(stats_path, stats)

    changed_stats = [item for item in stats if item["changes"]]
    print(f"Entities: {len(entities)}")
    print(f"State changes: {len(state_rows)}")
    print(f"Decoded payload states: {len(decoded_rows)}")
    print(f"Changing byte streams: {len(changed_stats)}")
    for path in (json_path, states_path, payload_path, stats_path):
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
