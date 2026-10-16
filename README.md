# SWS001 weather-station transmitter

PlatformIO firmware for replaying a captured SL22055/SWS001-like 433.90 MHz burst with an ESP32-WROOM and CC1101.

The firmware can replay a captured seven-row packet or encode a requested temperature from −1 to 41 °C. It does not yet read a temperature sensor.

## Wiring

| CC1101 | ESP32 |
|---|---:|
| VCC | 3.3 V |
| GND | GND |
| SCK | GPIO18 |
| MISO / SO | GPIO19 |
| MOSI / SI | GPIO23 |
| CSN | GPIO5 |
| GDO0 | GPIO4 |

## Build

Build in Docker; the named Compose volume caches the PlatformIO toolchain:

```sh
docker compose run --rm firmware
```

The resulting image is `.pio/build/esp32dev/firmware.bin`. Flash it with your usual ESP32 uploader, then open the serial monitor at 115200 baud.

Wait for:

```text
[CC1101] begin = 0
[CC1101] direct async = 0
Ready: tx
```

Send `tx` to replay the captured burst unchanged, or `tx <celsius>` (for example `tx 41` or `tx -0.5`) to encode and send one burst. Input must be a finite number between **−1 and 41 °C**; unsupported or malformed input sends nothing. The firmware updates B3, the wrap-dependent B4 control bits, and the checksum in all seven rows. The empirical model may differ from the station's displayed value by ~0.1 °C.

## Validate fixtures

The host test uses the same checksum implementation as the firmware:

```sh
c++ -std=c++11 -Iinclude tests/test_protocol.cpp -o /tmp/test_protocol
/tmp/test_protocol
```

## Verify RF with rtl_433

Keep OpenWebRX stopped while `rtl_433` owns the RTL-SDR, then run on the Raspberry Pi:

```sh
docker exec rtl433-debug rtl_433 \
  -d 0 -f 433.90M -s 2048000 -g 0 -R 0 \
  -X 'n=SWS001_like,m=OOK_PWM,s=508,l=1008,r=5024,g=0,t=0,y=3012' \
  -M time:iso -F json
```

A successful `tx` must decode to exactly these seven rows:

```text
a5d2fdea5f2dbbe9
a5d2fdea572dbbe1
a5d2fdea5b2dbbee
a5d2fdea532dbbe6
a5d2fdea5d2dbbea
a5d2fdea552dbbe2
a5d2fdea592dbbec
```

If decoding fails, capture the CC1101 output with `rtl_433 -A` and compare bit order, pulse timings, and measured carrier against the real-sensor baseline before changing RF settings.

## Current CC1101 calibration

The firmware transmits the captured bytes MSB-first using one hardware RMT write (eight memory blocks). At configured 433.940 MHz, a saved IQ capture measured the CC1101 peak at 433.901625 MHz; the real sensor's reference IQ peak was 433.901750 MHz. Output power is currently 5 dBm.

The real sensor uses two LOW gaps (~490/991 µs), **chosen according to the following HIGH bit**: LOW + next HIGH is ~1505 µs. Initially our ESP used the preceding bit, so `rtl_433` decoded the bytes while the station rejected the waveform. Correcting the gap phase made the station accept `tx` (HA 16.8 °C). Changing only B3 in the old B4=`5F` mode produced 20.0 °C for `tx20`, but **12.1 °C for nominal `tx23`** and 14.1 °C for nominal `tx25`: the station does not choose the nearest wrap based on its previous temperature. Captured real frames above 20 °C use B4=`9F`; switching that field in the diagnostic burst produced **23.0, 25.0, and 26.0 °C in HA** within ~0.5 s of each TX on 2026-10-16. The numeric encoder computes B3 from an empirical ~23.5-counts/°C model and uses the high two B4 bits to carry the modulo-256 wrap. Additional station tests on 2026-10-16: `tx 30` → HA **30.1 °C**, `tx 41` → **41.1 °C**, `tx 0` → **−0.1 °C**, and `tx -0.5` → **−0.6 °C**, each within ~0.6 s and with seven decoded 64-bit RF rows. **Do not extrapolate below −1 °C:** `tx -5` gave **38.6 °C** with both tried B4 variants (`1F` and `1E`), revealing an unresolved extra range distinction. Firmware rejects temperatures below −1 °C until a real cold-sensor capture resolves it. Logs: `/opt/rtl433-debug/captures/temperature-ramp-20261016/` and `/opt/rtl433-debug/captures/numeric-temperature-20261016/` on the Pi. IQ: `/opt/rtl433-debug/captures/compare-fixedgain-real-20261015/` and `/opt/rtl433-debug/captures/compare-gapphase-esp-20261015/`. Output power remains **5 dBm**.
