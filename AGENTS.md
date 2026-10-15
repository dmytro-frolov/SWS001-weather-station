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

## ESP32 + CC1101 wiring

- CSN: GPIO 5 (D5)
- GDO0: GPIO 4 (D4)
- MOSI: GPIO 23 (D23)
- SCK: GPIO 18 (D18)
- MISO: GPIO 19 (D19)
- Power: VCC -> 3.3V, GND -> GND

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

After changing the antenna and moving the real sensor closer, `rtl_433` decoded valid seven-row bursts and correlated 16.81 °C over RF with a 16.8 °C Home Assistant update about 1.4 seconds later. Reference IQ files are `/opt/rtl433-debug/captures/real-valid-iq/g009_433.9M_2048k.cu8` and `g057_433.9M_2048k.cu8`.

On 2026-10-15, reconnecting GPIO4 to CC1101 GDO0 exposed a byte-order error: rtl_433 received every replay byte bit-reversed. Firmware now sends MSB-first; rtl_433 verified seven exact 64-bit rows with valid checksums repeatedly. Configured 433.940 MHz yielded an IQ peak of 433.901625 MHz versus 433.901750 MHz for the real sensor. At 0 dBm, three complete replay bursts decoded (`/opt/rtl433-debug/captures/cc1101-20261015/sws001-power0-rf.jsonl`), but Home Assistant did not update. Power is now 5 dBm; `tx` still sends the original replay, while `tx25` changes only B3 to E9 and recalculates B7 for all seven rows. `rtl_433` verified all seven `tx25` rows (`sws001-tx25-rf.jsonl` in the same directory), predicting ~24.98 °C with a 25 °C reference. Temperature wraps modulo 256, so the same frame can also represent ~14.09 °C near ambient 16–17 °C. Home Assistant did not update after `tx25` either; station acceptance is still unproven. Compare strength at the station's location before modifying other control bytes.

## Working order

1. Capture a real `a5d2fd` 64-bit frame with `rtl_433`.
2. Save raw timing/IQ from the real sensor.
3. Capture the CC1101 waveform under identical settings.
4. Fix physical pulse shape and framing until bytes decode identically.
5. Then adjust temperature and checksum; keep known-good control bytes.
