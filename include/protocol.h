#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

namespace sws001 {

static const size_t kRowCount = 7;
static const size_t kRowSize = 8;

static const uint8_t kReplayRows[kRowCount][kRowSize] = {
    {0xA5, 0xD2, 0xFD, 0xEA, 0x5F, 0x2D, 0xBB, 0xE9},
    {0xA5, 0xD2, 0xFD, 0xEA, 0x57, 0x2D, 0xBB, 0xE1},
    {0xA5, 0xD2, 0xFD, 0xEA, 0x5B, 0x2D, 0xBB, 0xEE},
    {0xA5, 0xD2, 0xFD, 0xEA, 0x53, 0x2D, 0xBB, 0xE6},
    {0xA5, 0xD2, 0xFD, 0xEA, 0x5D, 0x2D, 0xBB, 0xEA},
    {0xA5, 0xD2, 0xFD, 0xEA, 0x55, 0x2D, 0xBB, 0xE2},
    {0xA5, 0xD2, 0xFD, 0xEA, 0x59, 0x2D, 0xBB, 0xEC},
};

inline uint8_t reverse8(uint8_t value) {
  value = (value >> 4) | (value << 4);
  value = ((value & 0xCC) >> 2) | ((value & 0x33) << 2);
  return ((value & 0xAA) >> 1) | ((value & 0x55) << 1);
}

inline uint8_t checksum(const uint8_t row[kRowSize]) {
  uint8_t sum = 6;
  for (size_t i = 0; i < kRowSize - 1; ++i) {
    sum += reverse8(row[i]);
  }
  return reverse8(sum);
}

inline void setTemperatureByte(uint8_t row[kRowSize], uint8_t onAirByte) {
  row[3] = onAirByte;
  row[7] = checksum(row);
}

inline bool setTemperature(uint8_t row[kRowSize], float celsius) {
  if (!isfinite(celsius) || celsius < -20.0f || celsius > 42.3f) {
    return false;
  }
  const long count = lroundf(482.0f - 23.5f * celsius);
  const int wrap = static_cast<int>(floorf(count / 256.0f));
  // shortcut: B4 aliases outside −20..42.3 C; capture real extremes before extending.
  const uint8_t mode = reverse8(static_cast<uint8_t>((wrap + 2) & 7)) & 0xE0;
  row[4] = (row[4] & 0x1F) | mode;
  setTemperatureByte(row, reverse8(static_cast<uint8_t>(count)));
  return true;
}

inline bool validateRow(const uint8_t row[kRowSize]) {
  return row[0] == 0xA5 && row[1] == 0xD2 && row[2] == 0xFD &&
         row[7] == checksum(row);
}

inline bool validateReplayRows() {
  for (size_t i = 0; i < kRowCount; ++i) {
    if (!validateRow(kReplayRows[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace sws001
