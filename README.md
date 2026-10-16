# SWS001 weather-station transmitter

PlatformIO firmware for replaying a captured SL22055/SWS001-like 433.90 MHz burst with an ESP32-WROOM and CC1101.

The firmware can replay a captured seven-row packet or transmit a diagnostic seven-row packet for approximately 25 °C. It does not yet read a temperature sensor.

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

Send `tx` to replay the captured seven-row burst, or `tx25` to transmit the temperature byte corresponding to ~24.98 °C with the captured control bytes and recalculated checksums. Either command sends one burst and prints `TX done`. Because the temperature field wraps, the station currently displays **14.1 °C** for `tx25` when its previous reading is near 17 °C.

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

The real sensor uses two LOW gaps (~490/991 µs), **chosen according to the following HIGH bit**: LOW + next HIGH is ~1505 µs. Initially our ESP used the preceding bit, so `rtl_433` decoded the bytes while the station rejected the waveform. After correcting this phase on 2026-10-16, the station accepted the ESP: `tx25` changed HA from 17.7 to 14.1 °C at 15:57:28 UTC; `tx` changed HA from 17.7 to 16.8 °C at 15:59:06 UTC, both within one second of transmission. The `tx25` wrap is empirical and must be resolved before relying on arbitrary synthetic temperatures. The fresh real-sensor IQ and post-fix ESP IQ are on the Pi under `/opt/rtl433-debug/captures/compare-fixedgain-real-20261015/` and `/opt/rtl433-debug/captures/compare-gapphase-esp-20261015/`. Output power remains **5 dBm**.
