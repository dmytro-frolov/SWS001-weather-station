#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <esp32-hal-rmt.h>
#include <string.h>

#include "protocol.h"

namespace {

const int kPinCs = 5;
const int kPinGdo0 = 4;
const int8_t kOutputPowerDbm = 5;
// 151 logical counts unwraps to ~24.98 C; other wraps differ by ~10.89 C.
const uint8_t kTest25Byte = 0xE9;
// IQ captures measured ~433.8949 MHz here; tune near the real sensor's ~433.9017 MHz.
const float kConfiguredFrequencyMhz = 433.940;

const uint16_t kShortHighUs = 515;
const uint16_t kLongHighUs = 1017;
const uint16_t kShortLowUs = 490;
const uint16_t kLongLowUs = 991;
const uint16_t kSyncHighUs = 3101;
const uint16_t kResetLowUs = 4991;

SPIClass spi(VSPI);
SPISettings spiSettings(200000, MSBFIRST, SPI_MODE0);
CC1101 radio = new Module(kPinCs, RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC,
                          spi, spiSettings);

const size_t kSymbolsPerRow = 65;
rmt_data_t txSymbols[sws001::kRowCount * kSymbolsPerRow];

void halt(const char* stage, int16_t state) {
  Serial.printf("[CC1101] %s = %d\n", stage, state);
  while (true) {
    delay(1000);
  }
}

void buildReplayBurst(bool test25) {
  size_t symbolIndex = 0;
  for (size_t rowIndex = 0; rowIndex < sws001::kRowCount; ++rowIndex) {
    uint8_t row[sws001::kRowSize];
    memcpy(row, sws001::kReplayRows[rowIndex], sizeof(row));
    if (test25) {
      sws001::setTemperatureByte(row, kTest25Byte);
    }
    txSymbols[symbolIndex++] = {kSyncHighUs, 1, kLongLowUs, 0};

    for (size_t bitIndex = 0; bitIndex < 8 * sws001::kRowSize; ++bitIndex) {
      const bool bit = (row[bitIndex / 8] >> (7 - bitIndex % 8)) & 1;
      const bool finalBit = bitIndex == 8 * sws001::kRowSize - 1;
      const size_t next = bitIndex + 1;
      const bool nextBit = !finalBit && ((row[next / 8] >> (7 - next % 8)) & 1);
      // The real sensor's LOW gap is paired with the following HIGH pulse.
      txSymbols[symbolIndex++] = {
          bit ? kShortHighUs : kLongHighUs,
          1,
          finalBit ? kResetLowUs : (nextBit ? kLongLowUs : kShortLowUs),
          0,
      };
    }
  }
}

bool sendReplayBurst() {
  return rmtWrite(kPinGdo0, txSymbols, RMT_SYMBOLS_OF(txSymbols), 2000);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!sws001::validateReplayRows()) {
    halt("invalid replay fixture", -1);
  }

  spi.begin(18, 19, 23, kPinCs);

  int16_t state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    halt("begin", state);
  }
  Serial.printf("[CC1101] begin = %d\n", state);

  state = radio.setFrequency(kConfiguredFrequencyMhz);
  if (state != RADIOLIB_ERR_NONE) {
    halt("frequency", state);
  }

  state = radio.setOOK(true);
  if (state != RADIOLIB_ERR_NONE) {
    halt("OOK", state);
  }

  state = radio.setOutputPower(kOutputPowerDbm);
  if (state != RADIOLIB_ERR_NONE) {
    halt("output power", state);
  }

  state = radio.transmitDirectAsync();
  if (state != RADIOLIB_ERR_NONE) {
    halt("direct async", state);
  }
  Serial.printf("[CC1101] direct async = %d\n", state);

  pinMode(kPinGdo0, OUTPUT);
  digitalWrite(kPinGdo0, LOW);
  if (!rmtInit(kPinGdo0, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_8, 1000000)) {
    halt("RMT init", -1);
  }
  if (!rmtSetEOT(kPinGdo0, LOW)) {
    halt("RMT EOT", -1);
  }
  Serial.println("Ready: tx");
}

void loop() {
  if (!Serial.available()) {
    return;
  }

  String command = Serial.readStringUntil('\n');
  command.trim();
  if (command != "tx" && command != "tx25") {
    Serial.println("Use: tx or tx25");
    return;
  }

  buildReplayBurst(command == "tx25");
  if (sendReplayBurst()) {
    Serial.println("TX done");
  } else {
    Serial.println("TX failed");
  }
}
