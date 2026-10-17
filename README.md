# SWS001 weather-station transmitter

PlatformIO firmware for replaying a captured SL22055/SWS001-like 433.90 MHz burst with a CC1101 and either an ESP32-WROOM or an ESP8266 D1 mini.

The firmware can replay a captured seven-row packet or encode a requested temperature (−20…42.3 °C) and optional humidity (0…99%). On the D1 mini it also reads a DS18B20 and automatically transmits its temperature every 128 seconds with fixed **0% humidity**. The ESP32 remains manual-only.

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

For a standard **ESP8266 D1 mini** instead:

| Device | D1 mini |
|---|---|
| CC1101 SCK / MISO / MOSI | D5 / D6 / D7 |
| CC1101 CSN / GDO0 | D1 / D2 |
| CC1101 VCC / GND | 3.3 V / GND |
| DS18B20 DATA | D4, 4.7 kΩ pull-up to 3.3 V |
| DS18B20 VCC / GND | 3.3 V / GND |

D4 must stay HIGH at boot. Use the DS18B20's three-wire power connection, not parasite power. The D1 mini stays awake between reports (USB power); reserve D0 for a future D0→RST deep-sleep wake connection. Never feed 5 V to CC1101.

## Build

Build in Docker; the named Compose volume caches the PlatformIO toolchain:

```sh
docker compose run --rm firmware                  # ESP32, unchanged default
docker compose run --rm firmware pio run -e d1_mini # ESP8266 D1 mini
```

The resulting images are `.pio/build/esp32dev/firmware.bin` and `.pio/build/d1_mini/firmware.bin`. Flash the appropriate image for your board, then open the serial monitor at 115200 baud.

Wait for:

```text
[CC1101] begin = 0
[CC1101] direct async = 0
Ready: tx
```

Send `tx` to replay the captured burst unchanged, `tx <celsius>` to change temperature while keeping the captured humidity byte, or `tx <celsius> <humidity-percent>` (for example `tx -5 73`) to set both. Valid ranges are **−20…42.3 °C** and **0…99%**; invalid input sends nothing. The firmware updates B3, wrap-dependent B4, optional B5, and the checksum in all seven rows. The empirical temperature model may differ from the station's display by ~0.1 °C.

On the D1 mini, the first automatic transmission follows the first completed DS18B20 conversion after boot; subsequent measurements start every **128 s** (the observed interval of the original sensor). Automatic packets set humidity to **0%**, not a measured value. Invalid/absent sensor readings, 85 °C at startup, and temperatures outside −20…42.3 °C are logged and **not transmitted**; the next interval retries. Manual `tx` commands continue to work, including unchanged exact replay without arguments. No deep sleep or battery optimization is implemented.

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

The ESP32 transmits the captured bytes MSB-first using one hardware RMT write (eight memory blocks); the D1 mini uses GPIO timing with Wi-Fi disabled. At configured 433.940 MHz, a saved ESP32 IQ capture measured the CC1101 peak at 433.901625 MHz; the real sensor's reference IQ peak was 433.901750 MHz. Output power is currently 5 dBm.

On 2026-10-17 the D1 mini's `tx 33 44` produced seven checksum-valid RF rows (~33.02 °C decoded); the station updated Home Assistant to **33.1 °C / 44%** after ~0.59 s. IQ pulse analysis gave ~521/1017 µs HIGH, ~989/494 µs LOW and LOW + next HIGH ~1511 µs. Captures: `/opt/rtl433-debug/captures/d1-mini-20261017/` on the Pi. The ESP32 was not connected by USB for a same-day side-by-side transmission.

Two automatic DS18B20 reports on 2026-10-17 were **128 s apart**: sensor **22.75 / 22.69 °C**, seven valid RF rows per burst (~22.77 / 22.68 °C, humidity byte `FF`), and HA **22.8 / 22.7 °C at 0% RH**. The first IQ capture showed LOW + next HIGH ~1511 µs; a subsequent reboot also transmitted automatically. Captures: `/opt/rtl433-debug/captures/d1-mini-ds18b20-20261017/`. The original sensor was still active and sent separate ~18 °C / 61% packets; disconnect it to avoid alternating readings on the shared station channel.

The real sensor uses two LOW gaps (~490/991 µs), **chosen according to the following HIGH bit**: LOW + next HIGH is ~1505 µs. Initially our ESP used the preceding bit, so `rtl_433` decoded the bytes while the station rejected the waveform. Correcting the gap phase made the station accept `tx` (HA 16.8 °C). Changing only B3 in the old B4=`5F` mode produced 20.0 °C for `tx20`, but **12.1 °C for nominal `tx23`** and 14.1 °C for nominal `tx25`: the station does not choose the nearest wrap based on its previous temperature. Captured real frames above 20 °C use B4=`9F`; switching that field in the diagnostic burst produced **23.0, 25.0, and 26.0 °C in HA** within ~0.5 s of each TX on 2026-10-16. The numeric encoder computes B3 from an empirical ~23.5-counts/°C model and uses the high three B4 bits to carry the modulo-256 wrap. Additional station tests on 2026-10-16: `tx 30` → HA **30.1 °C**, `tx 41` → **41.1 °C**, `tx 0` → **−0.1 °C**, and `tx -0.5` → **−0.6 °C**, each within ~0.6 s and with seven decoded 64-bit RF rows. Initially `tx -5` gave **38.6 °C** with B4=`1F` or `1E`. A real sensor cooled in the freezer yielded seven checksum-valid rows at both −14.7 and −15.1 °C with B4=`BF` (e.g. `a5d2fd1cbf6d0701`), revealing the missing **cold bit `0x20`**. With that bit, `tx -5` gave **−5.1 °C** (seven correct RF rows with B4=`3F`), `tx -18` gave **−18.1 °C** in HA, and `tx -20` gave **−20.0 °C** with seven valid RF rows. The validated *emulator* range is −20…42.3 °C (station displays up to ~42.4 °C): `tx -21` (same verified B4=`BF` as `tx -20`) and `tx -24` both displayed **−20.0 °C**; `tx 42.3` displayed **42.4 °C**, while `tx 42.4` was received but displayed **−20.0 °C** due to B4 range aliasing. Inputs outside the validated range are rejected. This does **not** prove the station's true hardware maximum above 42 °C; a real hotter-sensor frame may reveal another flag. Exact rounding still requires validation. Logs: `/opt/rtl433-debug/captures/temperature-ramp-20261016/`, `/opt/rtl433-debug/captures/numeric-temperature-20261016/`, `/opt/rtl433-debug/captures/freezer-20261016-1920/`, and `/opt/rtl433-debug/captures/limit-sweep-20261016/` on the Pi. IQ: `/opt/rtl433-debug/captures/compare-fixedgain-real-20261015/` and `/opt/rtl433-debug/captures/compare-gapphase-esp-20261015/`. Output power remains **5 dBm**.

## Button on the original sensor

Two button presses on 2026-10-17 caused immediate ordinary seven-row RF bursts with the low bit of B4 **clear** (`9E` in the first row); regular scheduled bursts had it **set** (`9F`). The automatic ~128 s schedule continued unchanged, including one regular burst five seconds after the second press. Both button bursts updated the weather station/HA, so the button is strongly consistent with **manual transmit/test**. The user reports a station beep on press, but the precise cause of the beep (the B4 bit versus a more general reception response) has not been isolated. Evidence: `/opt/rtl433-debug/captures/button-20261017T192449Z/` on the Pi.

## Humidity

For the tested station, `RH = floor((255 - reverse8(B5)) * 25 / 32)`. To encode a requested 0…99% RH, choose `count = ceil(RH * 32 / 25)`, then `B5 = reverse8(255 - count)`; recompute B7. Controlled ESP tests at a constant 16.0 °C produced **0, 11, 27, 36, 37, 42, 57, 58, 73, 74, 85, 97, 98, and 99%** in Home Assistant (`sensor.out_hum`). The old `244 - reverse8(B5)` estimate was wrong (e.g. raw B5=`89` gives 85%, not 99%). An input of 100% wrapped to 0% on the station, so firmware rejects it. Combined commands also worked: `tx -5 73` gave −5.1 °C / 73%, and `tx 30 37` gave 30.1 °C / 37% in HA. Only B5 and checksum change when varying humidity at a fixed requested temperature. Evidence: `/opt/rtl433-debug/captures/humidity-calibration-20261016/` on the Pi; the distant RTL-SDR did not decode those individual bursts, but each HA change followed a CC1101 transmission by ~0.5 s.
