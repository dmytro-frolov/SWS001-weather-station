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
}
