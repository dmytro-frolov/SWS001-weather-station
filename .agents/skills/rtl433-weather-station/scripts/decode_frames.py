#!/usr/bin/env python3
"""Validate rtl_433 JSON rows and decode SL22055-like temperatures."""

import argparse
import json
from pathlib import Path

PREFIX = bytes.fromhex("a5d2fd")


def reverse8(value: int) -> int:
    return int(f"{value:08b}"[::-1], 2)


def valid_frame(frame: bytes) -> bool:
    if len(frame) != 8 or frame[:3] != PREFIX:
        return False
    checksum = reverse8((sum(reverse8(value) for value in frame[:7]) + 6) & 0xFF)
    return frame[7] == checksum


def temperature(frame: bytes, reference: float) -> float:
    logical = reverse8(frame[3])
    candidates = [(482 - (logical + 256 * k)) / 23.5 for k in range(-4, 5)]
    return min(candidates, key=lambda value: abs(value - reference))


def frames(path: Path):
    for line in path.read_text().splitlines():
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        for row in record.get("rows", []):
            if row.get("len") != 64:
                continue
            try:
                frame = bytes.fromhex(row["data"])
            except (KeyError, ValueError):
                continue
            if valid_frame(frame):
                yield record.get("time", "unknown-time"), frame


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("jsonl", type=Path)
    parser.add_argument("--reference-c", type=float, required=True)
    args = parser.parse_args()

    found = False
    for timestamp, frame in frames(args.jsonl):
        found = True
        print(f"{timestamp} {frame.hex()} {temperature(frame, args.reference_c):.2f} C")
    if not found:
        raise SystemExit("No valid 64-bit a5d2fd frames found")


if __name__ == "__main__":
    main()
