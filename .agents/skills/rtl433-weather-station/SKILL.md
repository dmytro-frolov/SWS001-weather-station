---
name: rtl433-weather-station
description: Capture, validate, and decode SL22055/SWS001-like 433.90 MHz weather-sensor frames with rtl_433 on the project Raspberry Pi. Use for RF captures, balcony temperature readings, pulse-timing analysis, and comparison with CC1101 transmissions.
---

# rtl_433 weather station

Use the Raspberry Pi SDR host (`<user>@<sdr-host-ip>`) with the project key `./pi_dev`. The Docker container is `rtl433-debug`; captures live in `/opt/rtl433-debug/captures`.

## Safety and receiver ownership

There is one RTL-SDR. Never run OpenWebRX and `rtl_433` against USB simultaneously. Check `owrx-mbe` first, stop it for capture, and restore it only if it was running before the capture. Never print or commit `pi_dev`.

## Decode the weather sensor

Passive captures observed an automatic burst about every 128 seconds. Run for at least 180 seconds unless the sensor is triggered manually; use 240 seconds when reception is uncertain:

```sh
ssh -i ./pi_dev <user>@<sdr-host-ip> '
was_running=$(docker inspect -f "{{.State.Running}}" owrx-mbe 2>/dev/null || echo false)
restore() { [ "$was_running" = true ] && docker start owrx-mbe >/dev/null || true; }
trap restore EXIT INT TERM
[ "$was_running" = true ] && docker stop -t 15 owrx-mbe >/dev/null
docker start rtl433-debug >/dev/null
docker exec rtl433-debug timeout -s INT 180s rtl_433 \
  -d 0 -f 433.90M -s 2048000 -g 0 -R 0 \
  -X "n=SWS001_like,m=OOK_PWM,s=508,l=1008,r=5024,g=0,t=0,y=3012" \
  -M time:iso -F json
' > capture.jsonl
```

A target burst has seven 64-bit rows beginning with `a5d2fd`; every accepted row must also pass the additive checksum. Confirmed examples are:

```text
2026-10-14T23:18:13  a5d2fdb25f2d7b71  17.23 °C
2026-10-14T23:20:21  a5d2fdd25f2dbbd1  17.32 °C
```

Ignore zero/short rows and long runs such as `f`, `ff`, or `fff...`; those came from nearby interference. Do not enable `-Y autolevel` by default because the strong adjacent signal can produce a flood of false rows.

Decode with the live Home Assistant value as the modulo-256 reference:

```sh
reference=$(python3 .agents/skills/homeassistant-sensor/scripts/ha_sensor.py --temp-only)
python3 .agents/skills/rtl433-weather-station/scripts/decode_frames.py \
  capture.jsonl --reference-c "$reference"
```

`--reference-c` only selects the correct temperature wrap; it does not force the RF reading to equal Home Assistant. Candidates repeat about every 10.89 °C. The decoder rejects bad prefixes and checksums.

## Capture raw pulse timing

```sh
ssh -i ./pi_dev <user>@<sdr-host-ip> \
  '/opt/stacks/rtl433-debug/capture.sh real-sensor 180'
```

For CC1101, repeat with label `cc1101` while sending the firmware's unchanged `tx` replay, `tx <celsius>` (−20…42.3 °C), or `tx <celsius> <humidity-percent>` (0…99% RH) serial command. Compare each `analysis.log` and `.cu8` file under `/opt/rtl433-debug/captures`.

Expected nominal decoder timings are 508 µs and 1008 µs, sync 3012 µs, reset 5024 µs. Valid IQ captures measured about 515/1017 µs HIGH pulses, 991/490 µs LOW gaps, 3101 µs sync, and 4991 µs inter-row gaps. **The LOW gap depends on the following HIGH bit**, giving LOW+next HIGH ≈1505 µs; grouping HIGH+following LOW instead can decode correctly in rtl_433 but fail at the station. Treat measured RF timings as authoritative. Reference IQ files on the Pi are `real-valid-iq/g009_433.9M_2048k.cu8` and `g057_433.9M_2048k.cu8`.

## End-to-end verification

Follow this order and stop at the first failure:

1. Decode a real sensor burst and obtain the current Home Assistant reference value.
2. Transmit one captured seven-row burst unchanged through CC1101.
3. Require `rtl_433` to recover the same seven 64-bit rows with valid checksums.
4. Confirm that the existing weather station accepts the replay.
5. Confirm the corresponding Tuya/Home Assistant entity update.
6. Only then alter temperature bytes, recompute checksums, and finally feed live readings from the wired outdoor sensor.

Passing `rtl_433` proves the RF framing, not weather-station acceptance. A Home Assistant update is the final integration check.

## Current CC1101 status

The station accepted the ESP after correcting the LOW-gap phase (choose LOW for the *next* bit), despite rtl_433 decoding the old waveform's bytes too. Fresh real sensor on 2026-10-15 23:42:35 decoded ~16.55 °C; HA changed to 16.6 °C at 20:42:36 UTC. IQ: real ~433.9018 MHz, ESP ~433.9016 MHz; current CC1101 setting 433.940 MHz / 5 dBm. On 2026-10-16 `tx` changed HA 17.7→16.8 °C within one second of TX. B3 alone is insufficient to encode temperatures above ~20 °C: with the captured low B4 mode (`5F` first row), nominal `tx23` gave 12.1 °C even after `tx20` gave 20.0 °C; nominal `tx25` gave 14.1 °C. Real hot frames use B4=`9F` or `9E`. Switching B4 to the observed hot mode (`9F` first row, corresponding repeat sequence) while changing B3 and checksum made HA report **23.0, 25.0, and 26.0 °C** within ~0.5 s of transmission. Firmware accepts `tx` for exact replay, `tx <celsius>` to change only temperature, or `tx <celsius> <humidity-percent>` for −20…42.3 °C and 0…99% RH; separate numbered commands were removed. Confirmed RF/HA pairs include `tx 30`→30.1 °C, `tx 41`→41.1 °C, `tx 0`→−0.1 °C, `tx -0.5`→−0.6 °C. An initial `tx -5` wrongly displayed 38.6 °C with B4=`1F` or `1E`. Two real freezer captures at −14.7 and −15.1 °C both had B4=`BF` with changing B6 (`07` and `FB`), identifying an additional cold bit `0x20` for wraps >=2. With this bit, ESP `tx -5` displayed −5.1 °C (seven valid RF rows with B4=`3F`), `tx -18` displayed −18.1 °C in HA, and `tx -20` yielded −20.0 °C with seven valid RF rows. Boundary probes: `tx -21` (known B4=`BF`) and `tx -24` both updated HA to −20.0 °C, suggesting a lower display clamp. `tx 42.3` gave 42.4 °C; candidate `tx 42.4` (B4=`FF`) gave −20.0 °C, a range alias rather than a proven upper hardware limit. Outside −20…42.3 °C the final firmware refuses to transmit. For higher values capture a real heated sensor rather than extending B4 speculatively. Do not extrapolate below −20 °C without additional real cold-sensor evidence. RF/HA files: `/opt/rtl433-debug/captures/temperature-ramp-20261016/`, `/opt/rtl433-debug/captures/numeric-temperature-20261016/`, `/opt/rtl433-debug/captures/freezer-20261016-1920/`, and `/opt/rtl433-debug/captures/limit-sweep-20261016/`. A beep was heard during tests, but its cause is unknown. This B4 inference is validated only for the tested range; do not extrapolate its other meanings. IQ evidence: `/opt/rtl433-debug/captures/compare-fixedgain-real-20261015/` and `/opt/rtl433-debug/captures/compare-gapphase-esp-20261015/`.

## Humidity and protocol confidence

The station exposes `sensor.out_hum` in Home Assistant. Humidity is `floor((255 - reverse8(B5)) * 25 / 32)` for 0…99%; encode `count = ceil(RH * 32 / 25)` and `B5 = reverse8(255 - count)`, then recalculate B7. With the same transmitted temperature/control bytes, changing B5 alone caused HA humidity updates to 0, 11, 27, 36, 37, 42, 57, 58, 73, 74, 85, 97, 98 and 99%. A requested 100% instead displayed 0%, so firmware rejects 100. Combined tests `tx -5 73` → −5.1 °C / 73% and `tx 30 37` → 30.1 °C / 37% in HA. Logs: `/opt/rtl433-debug/captures/humidity-calibration-20261016/`. The distant RTL-SDR did not decode these individual bursts; HA confirmed station acceptance. The older `244 - reverse8(B5)` and PDF humidity page calibrations are contradicted by this controlled experiment. The additive checksum is established; temperature remains an empirical model. B4/B6 meanings outside tested ranges remain provisional.
