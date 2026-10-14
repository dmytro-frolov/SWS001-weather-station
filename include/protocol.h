#pragma once

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
