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

On 2026-10-16 a fresh real-sensor seven-row burst at 23:42:35 (~16.55 °C) correlated with an HA update to 16.6 °C at 20:42:36 UTC. IQ measured the real carrier ~433.9018 MHz versus ESP ~433.9016 MHz. The blocking TX flaw was **LOW-gap phase**, not a fixed LOW gap: on the real sensor, 490/991 µs LOW is selected by the *following* 1017/515 µs HIGH bit (LOW + next HIGH ~1505 µs). The old ESP selected by the preceding bit; rtl_433 decoded exact bytes but the station did not accept them. Firmware now sends MSB-first with the corrected gap phase, configured 433.940 MHz / 5 dBm. Station acceptance is proven: before correcting B4, nominal `tx25` (low B4 mode) changed HA 17.7→14.1 °C at 15:57:28 UTC, ~0.45 s after TX started; `tx` changed HA 17.7→16.8 °C at 15:59:06 UTC, ~0.56 s after TX started. Further testing showed the station does **not** choose the nearest wrap based on its last temperature: with B4=`5F`, `tx20` gave 20.0 °C but nominal `tx23` gave 12.1 °C, even immediately after `tx20`. Captured real hot frames use B4=`9F` (or `9E`) rather than low-mode `5F`. Switching the first row's B4 from `5F` to `9F` and correspondingly all seven repeat rows (xor `C0`), keeping B5/B6 unchanged and recalculating B7, gave **23.0, 25.0, and 26.0 °C in HA** within ~0.5 s of each TX on 2026-10-16. Firmware now keeps `tx` unchanged for replay and uses `tx <celsius>` with strict parsing for −1…41 °C (separate `tx20` etc. commands removed). It computes B3 with the empirical 23.5-counts/°C model and selects the observed B4 wrap mode for all seven rows before recomputing B7. Further RF/HA tests: `tx 30` → 30.1 °C, `tx 41` → 41.1 °C, `tx 0` → −0.1 °C, `tx -0.5` → −0.6 °C; each had seven decoded RF rows and HA updated within ~0.6 s. **Below −1 °C is UNSUPPORTED:** `tx -5` incorrectly produced 38.6 °C with both B4=`1F` and `1E`; restoring with `tx` gave 16.8 °C. Capture the original sensor while actually below −1.3 °C and compare B4/B6 plus HA before allowing colder values. Station emitted an audible beep at an unspecified point during hot/cold tests; cause unconfirmed (alarm vs protocol/control), avoid assuming B4's other meanings. RF/HA logs on Pi: `/opt/rtl433-debug/captures/temperature-ramp-20261016/` and `/opt/rtl433-debug/captures/numeric-temperature-20261016/`. IQ: `/opt/rtl433-debug/captures/compare-fixedgain-real-20261015/` and `/opt/rtl433-debug/captures/compare-gapphase-esp-20261015/`.

## Working order

1. Capture a real `a5d2fd` 64-bit frame with `rtl_433`.
2. Save raw timing/IQ from the real sensor.
3. Capture the CC1101 waveform under identical settings.
4. Match physical pulse shape, including LOW-gap phase, not just bytes decoded by rtl_433.
5. Validate weather-station/HA acceptance for each new B4 range; numeric encoding is bounded to −1…41 °C, and cold values below −1 °C need original-sensor evidence before enabling.
