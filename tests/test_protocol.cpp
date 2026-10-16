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
  const uint8_t coldRows[][sws001::kRowSize] = {
      {0xA5, 0xD2, 0xFD, 0x1C, 0xBF, 0x6D, 0x07, 0x01},
      {0xA5, 0xD2, 0xFD, 0x82, 0xBF, 0x6D, 0xFB, 0x11},
  };
  for (const auto& row : coldRows) {
    assert(sws001::validateRow(row));
  }
  const float invalidValues[] = {-21.0f, 42.4f, 43.0f, NAN, INFINITY};
  for (float invalid : invalidValues) {
    uint8_t row[sws001::kRowSize];
    memcpy(row, sws001::kReplayRows[0], sizeof(row));
    assert(!sws001::setTemperature(row, invalid));
    assert(memcmp(row, sws001::kReplayRows[0], sizeof(row)) == 0);
  }
}
