---
name: homeassistant-sensor
description: Query Home Assistant for current sensor temperature and state. Use to obtain live reference temperature for SL22055/SWS001-like modulo-256 frame decoding, or to monitor temperature changes during captures.
---

# Home Assistant Reference Sensor

Reads current state and temperature from Home Assistant REST API using credentials stored in `.env`.

## Configuration

Credentials in `.env` (gitignored):

```sh
HA_URL="http://<ha-host>:8123"
HA_TOKEN="<long-lived-access-token>"
HA_ENTITY_ID="sensor.out_temp"
```

Never commit `.env` or display the token.

## Usage

Check current temperature and timestamp:

```sh
python3 .agents/skills/homeassistant-sensor/scripts/ha_sensor.py
```

Get raw float value (suitable for shell subcommands):

```sh
python3 .agents/skills/homeassistant-sensor/scripts/ha_sensor.py --temp-only
```

Pass directly as reference temperature into `decode_frames.py`:

```sh
python3 .agents/skills/rtl433-weather-station/scripts/decode_frames.py capture.jsonl \
  --reference-c $(python3 .agents/skills/homeassistant-sensor/scripts/ha_sensor.py --temp-only)
```

Inspect full sensor attributes as JSON:

```sh
python3 .agents/skills/homeassistant-sensor/scripts/ha_sensor.py --json
```
