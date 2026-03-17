#include "hexaos.h"
#include "esp_chip_info.h"

void EspPrintChipInfo() {
  esp_chip_info_t info;
  esp_chip_info(&info);

  LogInfo("Chip model: %s", CONFIG_IDF_TARGET);
  LogInfo("Chip rev:   %d", info.revision);
  LogInfo("CPU cores:  %d", info.cores);
  LogInfo("Flash size: %u KB", ESP.getFlashChipSize() / 1024);
}
