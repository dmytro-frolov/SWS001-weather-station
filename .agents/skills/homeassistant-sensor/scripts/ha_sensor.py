#!/usr/bin/env python3
"""Fetch current sensor state from Home Assistant REST API."""

import argparse
import json
import os
import sys
import urllib.request
from pathlib import Path


def load_env(env_path: Path) -> dict:
    env = {}
    if not env_path.is_file():
        return env
    for line in env_path.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            k, v = line.split("=", 1)
            env[k.strip()] = v.strip().strip("\"'")
    return env


def fetch_sensor(url: str, token: str, entity_id: str) -> dict:
    req = urllib.request.Request(
        f"{url.rstrip('/')}/api/states/{entity_id}",
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
        },
    )
    with urllib.request.urlopen(req, timeout=10) as resp:
        return json.loads(resp.read().decode())


def main() -> None:
    parser = argparse.ArgumentParser(description="Query Home Assistant sensor state.")
    parser.add_argument("--env-file", type=Path, default=Path(".env"))
    parser.add_argument("--entity-id", help="Override HA_ENTITY_ID")
    parser.add_argument("--temp-only", action="store_true", help="Print temperature float only")
    parser.add_argument("--json", action="store_true", help="Print raw HA JSON response")
    args = parser.parse_args()

    env = load_env(args.env_file)
    url = os.environ.get("HA_URL") or env.get("HA_URL")
    token = os.environ.get("HA_TOKEN") or env.get("HA_TOKEN")
    entity_id = args.entity_id or os.environ.get("HA_ENTITY_ID") or env.get("HA_ENTITY_ID")

    if not url or not token or not entity_id:
        sys.exit("Error: HA_URL, HA_TOKEN, and HA_ENTITY_ID must be set in .env or environment")

    data = fetch_sensor(url, token, entity_id)

    if args.json:
        print(json.dumps(data, ensure_ascii=False, indent=2))
    elif args.temp_only:
        print(float(data["state"]))
    else:
        unit = data.get("attributes", {}).get("unit_of_measurement", "")
        updated = data.get("last_updated", "unknown")
        print(f"{data.get('entity_id')}: {data.get('state')} {unit} (updated: {updated})")


if __name__ == "__main__":
    main()
