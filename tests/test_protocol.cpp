#include <assert.h>
#include <string.h>

#include "protocol.h"

int main() {
  assert(sws001::validateReplayRows());

  uint8_t corrupted[sws001::kRowSize];
  memcpy(corrupted, sws001::kReplayRows[0], sizeof(corrupted));
  corrupted[3] ^= 1;
  assert(!sws001::validateRow(corrupted));
}
