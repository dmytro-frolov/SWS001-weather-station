# Project agent guide

## Goal
Create Device based on cc1101 that will read temperature and transmit it to weather station. In order to check wether temperature is transmitted use skill rtl433-weather-station and skill homeassistant-sensor.
Reproduce the SL22055-marked balcony weather sensor with ESP32 + CC1101. First make `rtl_433` decode the transmitted RF waveform exactly; only then change payload bytes or test weather-station acceptance.

## Raspberry Pi SDR host

- SSH: `pi@192.168.10.30` using `./pi_dev`
- Dockge: `http://192.168.10.30:5001/`
- Debug stack: `/opt/stacks/rtl433-debug/compose.yaml`
- Debug container: `rtl433-debug`
- Captures: `/opt/rtl433-debug/captures//`
- One RTL-SDR is shared with `owrx-mbe`; stop OpenWebRX during capture and restore only its previous state.
- Never display or commit the private key `pi_dev`.

Load `.agents/skills/rtl433-weather-station/SKILL.md` for capture and temperature-decoding commands.

## Reference sensor (Home Assistant)

- Credentials in `.env` (`HA_URL`, `HA_TOKEN`, `HA_ENTITY_ID`). Never commit `.env` or display tokens.
- Load `.agents/skills/homeassistant-sensor/SKILL.md` to query live reference temperature and state.
- Use `ha_sensor.py --temp-only` to feed `--reference-c` into `decode_frames.py` and resolve modulo-256 temperature ambiguity.

## Established RF settings

- Frequency: 433.90 MHz
- OOK PWM, 64-bit rows
- Nominal pulses: 508/1008 µs
- Sync: 3012 µs
- Reset: 5024 µs
- Flex decoder name `SWS001_like` is local and not a confirmed product identity.

## Evidence and confidence

The byte-reversed additive checksum is validated. The temperature mapping is a strong empirical model but wraps modulo 256, so decoding needs a nearby reference temperature (available from Home Assistant). B4/B6 meanings and humidity calibration remain provisional.

The PDF `sl22055_protocol_en-1.pdf` is useful background but predates corrected qualifications in the handoff documents. Do not treat its B4 or page-dependent humidity claims as established.

## Current capture status

A 180-second analyzer capture on the Raspberry Pi found a separate strong periodic carrier roughly 220–230 kHz above 433.90 MHz, not a valid sensor PWM frame. Flex-decoder runs with default and automatic thresholds produced only fragmented 0–12-bit rows and no 64-bit row. No new valid balcony frame has yet been captured.

## Working order

1. Capture a real `a5d2fd` 64-bit frame with `rtl_433`.
2. Save raw timing/IQ from the real sensor.
3. Capture the CC1101 waveform under identical settings.
4. Fix physical pulse shape and framing until bytes decode identically.
5. Then adjust temperature and checksum; keep known-good control bytes.
