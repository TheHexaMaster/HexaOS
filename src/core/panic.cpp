#include "hexaos.h"

void Panic(const char* reason) {
  Serial.begin(115200);
  delay(10);

  LogError("PANIC: %s", reason ? reason : "unknown");

  while (true) {
    delay(1000);
  }
}
