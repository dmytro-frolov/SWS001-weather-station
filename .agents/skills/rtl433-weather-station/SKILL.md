---
name: rtl433-weather-station
description: Capture, validate, and decode SL22055/SWS001-like 433.90 MHz weather-sensor frames with rtl_433 on the project Raspberry Pi. Use for RF captures, balcony temperature readings, pulse-timing analysis, and comparison with CC1101 transmissions.
---

# rtl_433 weather station

Use the Raspberry Pi at `pi@192.168.10.30` with the project key `./pi_dev`. The Docker container is `rtl433-debug`; captures live in `/opt/rtl433-debug/captures`.

## Safety and receiver ownership

There is one RTL-SDR. Never run OpenWebRX and `rtl_433` against USB simultaneously. Check `owrx-mbe` first, stop it for capture, and restore it only if it was running before the capture. Never print or commit `pi_dev`.

## Decode the weather sensor

Passive captures observed an automatic burst about every 128 seconds. Run for at least 180 seconds unless the sensor is triggered manually; use 240 seconds when reception is uncertain:

```sh
ssh -i ./pi_dev pi@192.168.10.30 '
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
ssh -i ./pi_dev pi@192.168.10.30 \
  '/opt/stacks/rtl433-debug/capture.sh real-sensor 180'
```

For CC1101, repeat with label `cc1101` while sending the firmware's `tx` or `tx25` serial command. Compare each `analysis.log` and `.cu8` file under `/opt/rtl433-debug/captures`.

Expected nominal decoder timings are 508 µs and 1008 µs, sync 3012 µs, reset 5024 µs. Valid 2026-10-14 IQ captures measured about 515/1017 µs HIGH pulses, 991/490 µs LOW gaps, 3101 µs sync, and 4991 µs inter-row gaps. Treat measured RF timings as authoritative. Reference IQ files on the Pi are `real-valid-iq/g009_433.9M_2048k.cu8` and `g057_433.9M_2048k.cu8`.

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

After reconnecting GPIO4 to GDO0 and switching firmware to MSB-first bit order, `rtl_433` repeatedly decoded the seven exact replay rows with valid checksums. IQ measurement: at configured 433.940 MHz the CC1101 peaked at 433.901625 MHz versus 433.901750 MHz for the real sensor. Current power is 5 dBm. `tx25` preserves the captured control bytes, uses B3=E9, and recalculates B7; seven matching checksum-valid RF rows are saved in `/opt/rtl433-debug/captures/cc1101-20261015/sws001-tx25-rf.jsonl`. The empirical model predicts ~24.98 °C with a 25 °C reference, but modulo wrap also permits ~14.09 °C near ambient. Home Assistant did not update after this test; weather-station acceptance is **not established**. Check strength/placement at the station before changing control bytes.

## Protocol confidence

The additive checksum is established. Temperature is an empirical model. The PDF's humidity page calibration and broad B4 interpretation are older hypotheses; prefer the qualifications in the handoff documents.
