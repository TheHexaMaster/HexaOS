#include "hexaos.h"
#include <FS.h>
#include <LittleFS.h>

bool EspLittlefsMount() {
  if (LittleFS.begin(true)) {
    LogInfo("LittleFS mount OK");
    return true;
  }

  LogError("LittleFS mount failed");
  return false;
}
