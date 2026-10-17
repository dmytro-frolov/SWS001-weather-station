#include <assert.h>
#include <string.h>

#include "protocol.h"

int main() {
  assert(sws001::validateReplayRows());

  uint8_t corrupted[sws001::kRowSize];
  memcpy(corrupted, sws001::kReplayRows[0], sizeof(corrupted));
  corrupted[3] ^= 1;
  assert(!sws001::validateRow(corrupted));

  const uint8_t expectedChecksums[sws001::kRowCount] = {
      0xEB, 0xE3, 0xED, 0xE5, 0xE9, 0xE1, 0xEE,
  };
  for (size_t i = 0; i < sws001::kRowCount; ++i) {
    uint8_t row[sws001::kRowSize];
    memcpy(row, sws001::kReplayRows[i], sizeof(row));
    sws001::setTemperatureByte(row, 0xE9);
    assert(sws001::validateRow(row));
    assert(row[3] == 0xE9 && row[7] == expectedChecksums[i]);
    assert(sws001::kReplayRows[i][3] == 0xEA);
  }

  const struct {
    float celsius;
    uint8_t b3, b4, checksum;
  } samples[] = {
      {20, 0x30, 0x5F, 0x32},  {23, 0xA3, 0x9F, 0x20},
      {30, 0x84, 0x9F, 0x06},  {31.5, 0x7F, 0x1F, 0x3C},
      {41, 0x78, 0x1F, 0x3A},  {9.5, 0xC0, 0xDF, 0x22},
      {0, 0x47, 0xDF, 0xC4},   {-0.5, 0x77, 0xDF, 0xF4},
      {-1, 0x5F, 0xDF, 0xDC},  {-1.5, 0xA0, 0x3F, 0xE2},
      {-5, 0x1A, 0x3F, 0x59},  {-14, 0xD4, 0xBF, 0x76},
      {-20, 0x1D, 0xBF, 0xDF}, {42.3f, 0x00, 0x1F, 0x7C},
  };
  for (const auto& sample : samples) {
    uint8_t row[sws001::kRowSize];
    memcpy(row, sws001::kReplayRows[0], sizeof(row));
    assert(sws001::setTemperature(row, sample.celsius));
    assert(row[3] == sample.b3 && row[4] == sample.b4 && row[7] == sample.checksum);
    assert(sws001::validateRow(row));
  }
  uint8_t combined[sws001::kRowSize];
  memcpy(combined, sws001::kReplayRows[0], sizeof(combined));
  assert(sws001::setTemperature(combined, 33));
  assert(sws001::setHumidity(combined, 44));
  const uint8_t expected33And44[] = {0xA5, 0xD2, 0xFD, 0x5B, 0x1F, 0x63, 0xBB, 0x54};
  assert(memcmp(combined, expected33And44, sizeof(combined)) == 0);
  memcpy(combined, sws001::kReplayRows[0], sizeof(combined));
  assert(sws001::setTemperature(combined, 18.125f));
  assert(sws001::setHumidity(combined, 0));
  assert(combined[5] == 0xFF && sws001::validateRow(combined));

  const uint8_t humidityBytes[] = {0x0B, 0x6D, 0x89};
  const uint8_t humidityChecksums[] = {0xCD, 0x99, 0x2E};
  for (size_t i = 0; i < sizeof(humidityBytes); ++i) {
    uint8_t row[sws001::kRowSize];
    memcpy(row, sws001::kReplayRows[0], sizeof(row));
    row[5] = humidityBytes[i];
    row[7] = sws001::checksum(row);
    assert(row[7] == humidityChecksums[i] && sws001::validateRow(row));
  }
  for (int humidity = 0; humidity <= 99; ++humidity) {
    uint8_t row[sws001::kRowSize];
    memcpy(row, sws001::kReplayRows[0], sizeof(row));
    assert(sws001::setHumidity(row, humidity));
    assert(((255 - sws001::reverse8(row[5])) * 25) / 32 == humidity);
    assert(sws001::validateRow(row));
    if (humidity == 85) {
      assert(row[5] == 0x49 && row[7] == 0xAE);
    }
  }
  const int invalidHumidity[] = {-1, 100};
  for (int invalid : invalidHumidity) {
    uint8_t row[sws001::kRowSize];
    memcpy(row, sws001::kReplayRows[0], sizeof(row));
    assert(!sws001::setHumidity(row, invalid));
    assert(memcmp(row, sws001::kReplayRows[0], sizeof(row)) == 0);
  }
  const uint8_t coldRows[][sws001::kRowSize] = {
      {0xA5, 0xD2, 0xFD, 0x1C, 0xBF, 0x6D, 0x07, 0x01},
      {0xA5, 0xD2, 0xFD, 0x82, 0xBF, 0x6D, 0xFB, 0x11},
  };
  for (const auto& row : coldRows) {
    assert(sws001::validateRow(row));
  }
  const float invalidValues[] = {-127.0f, -21.0f, 42.4f, 43.0f, 85.0f, NAN, INFINITY};
  for (float invalid : invalidValues) {
    uint8_t row[sws001::kRowSize];
    memcpy(row, sws001::kReplayRows[0], sizeof(row));
    assert(!sws001::setTemperature(row, invalid));
    assert(memcmp(row, sws001::kReplayRows[0], sizeof(row)) == 0);
  }
}
