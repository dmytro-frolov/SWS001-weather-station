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

## ESP8266 D1 mini port (2026-10-17)

D1 mini on `/dev/cu.usbserial-212110` is an ESP8266EX. CC1101 wiring: D5 SCK, D6 MISO, D7 MOSI, D1 CSN, D2 GDO0, 3.3 V/GND; optional DS18B20 DATA D4 with a 4.7 kΩ pull-up to 3.3 V (not yet read by firmware). D4 must stay HIGH during boot; reserve D0 for a future D0→RST deep-sleep wake link. `docker compose run --rm firmware pio run -e d1_mini` builds the ESP8266 firmware; the default Docker build still targets ESP32. ESP8266 has no RMT: the port uses GPIO delays with Wi-Fi off. Flashed D1 mini, then `tx 33 44` produced seven checksum-valid rows (~33.02 °C RF) and HA **33.1 °C / 44%** ~0.59 s after TX start; SDR IQ showed ~521/1017 µs HIGH, ~989/494 µs LOW and LOW + next HIGH ~1511 µs. Evidence: `/opt/rtl433-debug/captures/d1-mini-20261017/` on the Pi. ESP32 was not connected for a same-day side-by-side control TX. Battery draw and DS18B20 reading remain untested/unimplemented.

## Established RF settings

- Frequency: 433.90 MHz
- OOK PWM, 64-bit rows
- Nominal pulses: 508/1008 µs
- Sync: 3012 µs
- Reset: 5024 µs
- Flex decoder name `SWS001_like` is local and not a confirmed product identity.

## Evidence and confidence

The byte-reversed additive checksum is validated. The temperature mapping is a strong empirical model but wraps modulo 256, so decoding needs a nearby reference temperature (available from Home Assistant). B4/B6 full meanings remain provisional. **Humidity is now empirically validated:** `RH = floor((255 - reverse8(B5)) * 25 / 32)` for 0…99%; encode `count = ceil(RH * 32 / 25)`, `B5 = reverse8(255 - count)`, recalculate B7. At a fixed 16.0 °C, changing only B5 and checksum produced exact HA `sensor.out_hum` updates for 0, 11, 27, 36, 37, 42, 57, 58, 73, 74, 85, 97, 98 and 99%. Requested 100% displayed 0% (wrap), so firmware rejects 100. Combined tests `tx -5 73` → −5.1 °C / 73% and `tx 30 37` → 30.1 °C / 37% confirm simultaneous encoding. Old `244 - reverse8(B5)` predictions are wrong (e.g. B5=`89` is 85%, not 99%). Evidence: `/opt/rtl433-debug/captures/humidity-calibration-20261016/`; the SDR at its distant location did not decode these individual bursts, but HA updated ~0.5 s after CC1101 transmission.

The PDF `sl22055_protocol_en-1.pdf` is useful background but predates corrected qualifications in the handoff documents. Do not treat its B4 or page-dependent humidity claims as established.

## Current capture status

After changing the antenna and moving the real sensor closer, `rtl_433` decoded valid seven-row bursts and correlated 16.81 °C over RF with a 16.8 °C Home Assistant update about 1.4 seconds later. Reference IQ files are `/opt/rtl433-debug/captures/real-valid-iq/g009_433.9M_2048k.cu8` and `g057_433.9M_2048k.cu8`.

On 2026-10-16 a fresh real-sensor seven-row burst at 23:42:35 (~16.55 °C) correlated with an HA update to 16.6 °C at 20:42:36 UTC. IQ measured the real carrier ~433.9018 MHz versus ESP ~433.9016 MHz. The blocking TX flaw was **LOW-gap phase**, not a fixed LOW gap: on the real sensor, 490/991 µs LOW is selected by the *following* 1017/515 µs HIGH bit (LOW + next HIGH ~1505 µs). The old ESP selected by the preceding bit; rtl_433 decoded exact bytes but the station did not accept them. Firmware now sends MSB-first with the corrected gap phase, configured 433.940 MHz / 5 dBm. Station acceptance is proven: before correcting B4, nominal `tx25` (low B4 mode) changed HA 17.7→14.1 °C at 15:57:28 UTC, ~0.45 s after TX started; `tx` changed HA 17.7→16.8 °C at 15:59:06 UTC, ~0.56 s after TX started. Further testing showed the station does **not** choose the nearest wrap based on its last temperature: with B4=`5F`, `tx20` gave 20.0 °C but nominal `tx23` gave 12.1 °C, even immediately after `tx20`. Captured real hot frames use B4=`9F` (or `9E`) rather than low-mode `5F`. Switching the first row's B4 from `5F` to `9F` and correspondingly all seven repeat rows (xor `C0`), keeping B5/B6 unchanged and recalculating B7, gave **23.0, 25.0, and 26.0 °C in HA** within ~0.5 s of each TX on 2026-10-16. Firmware keeps `tx` unchanged for replay and accepts `tx <celsius>` or `tx <celsius> <humidity-percent>` with strict bounds −20…42.3 °C and 0…99% (separate numbered commands removed). Without the humidity argument it preserves captured B5. It computes B3 with the empirical 23.5-counts/°C model and selects the observed B4 wrap mode for all seven rows before recomputing B7. Further RF/HA tests: `tx 30` → 30.1 °C, `tx 41` → 41.1 °C, `tx 0` → −0.1 °C, `tx -0.5` → −0.6 °C; each had seven decoded RF rows and HA updated within ~0.6 s. An initial `tx -5` incorrectly produced 38.6 °C with B4=`1F` or `1E`. On 2026-10-16 the real sensor in the freezer produced two checksum-valid seven-row bursts at −14.7 and −15.1 °C, both B4=`BF`, despite B6 changing from `07` to `FB` (`/opt/rtl433-debug/captures/freezer-20261016-1920/real.jsonl`). This exposed an extra cold bit `0x20` in B4. After enabling it for wrap >=2, `tx -5` with B4=`3F` produced −5.1 °C and seven exact RF rows; `tx -18` with B4=`BF` produced −18.1 °C in HA, each ~0.43 s after TX; `tx -20` yielded −20.0 °C and seven valid RF rows. The current numeric range is **−20…42.3 °C**; values outside are rejected. Limit sweep: `tx -21` with the already validated B4=`BF` and `tx -24` with B4=`7F` both produced **−20.0 °C**; `tx 42.3` with B4=`1F` gave **42.4 °C**, while `tx 42.4` with candidate B4=`FF` updated HA to **−20.0 °C** (range alias, not a missing transmission). So −20 is the observed lower display clamp; 42.3 °C is the highest safely encoded requested value, not a proven hardware upper limit. Station emitted an audible beep at an unspecified point during hot/cold tests; cause unconfirmed (alarm vs protocol/control), avoid assuming B4's other meanings. RF/HA logs on Pi: `/opt/rtl433-debug/captures/temperature-ramp-20261016/`, `/opt/rtl433-debug/captures/numeric-temperature-20261016/`, `/opt/rtl433-debug/captures/freezer-20261016-1920/`, and `/opt/rtl433-debug/captures/limit-sweep-20261016/`. IQ: `/opt/rtl433-debug/captures/compare-fixedgain-real-20261015/` and `/opt/rtl433-debug/captures/compare-gapphase-esp-20261015/`.

## Working order

1. Capture a real `a5d2fd` 64-bit frame with `rtl_433`.
2. Save raw timing/IQ from the real sensor.
3. Capture the CC1101 waveform under identical settings.
4. Match physical pulse shape, including LOW-gap phase, not just bytes decoded by rtl_433.
5. Validate weather-station/HA acceptance for each new B4 range; numeric encoding is bounded to −20…42.3 °C. Do not equate the alias at 42.4 °C with a proven station hardware maximum; a real frame from a heated sensor is needed for higher values.
