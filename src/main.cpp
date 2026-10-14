#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

#include "protocol.h"

namespace {

const int kPinCs = 5;
const int kPinGdo0 = 4;

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

void halt(const char* stage, int16_t state) {
  Serial.printf("[CC1101] %s = %d\n", stage, state);
  while (true) {
    delay(1000);
  }
}

inline void level(bool high, uint16_t durationUs) {
  digitalWrite(kPinGdo0, high ? HIGH : LOW);
  delayMicroseconds(durationUs);
}

void sendRow(const uint8_t row[sws001::kRowSize]) {
  level(true, kSyncHighUs);
  level(false, kLongLowUs);

  for (size_t byteIndex = 0; byteIndex < sws001::kRowSize; ++byteIndex) {
    for (uint8_t bitIndex = 0; bitIndex < 8; ++bitIndex) {
      const bool bit = (row[byteIndex] >> bitIndex) & 1;
      const bool finalBit = byteIndex == sws001::kRowSize - 1 && bitIndex == 7;

      level(true, bit ? kShortHighUs : kLongHighUs);
      level(false, finalBit ? kResetLowUs
                            : (bit ? kLongLowUs : kShortLowUs));
    }
  }
}

void sendReplayBurst() {
  for (size_t i = 0; i < sws001::kRowCount; ++i) {
    sendRow(sws001::kReplayRows[i]);
  }
  digitalWrite(kPinGdo0, LOW);
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

  state = radio.setFrequency(433.90);
  if (state != RADIOLIB_ERR_NONE) {
    halt("frequency", state);
  }

  state = radio.setOOK(true);
  if (state != RADIOLIB_ERR_NONE) {
    halt("OOK", state);
  }

  state = radio.transmitDirectAsync();
  if (state != RADIOLIB_ERR_NONE) {
    halt("direct async", state);
  }
  Serial.printf("[CC1101] direct async = %d\n", state);

  pinMode(kPinGdo0, OUTPUT);
  digitalWrite(kPinGdo0, LOW);
  Serial.println("Ready: tx");
}

void loop() {
  if (!Serial.available()) {
    return;
  }

  String command = Serial.readStringUntil('\n');
  command.trim();
  if (command != "tx") {
    Serial.println("Use: tx");
    return;
  }

  sendReplayBurst();
  Serial.println("TX done");
}
