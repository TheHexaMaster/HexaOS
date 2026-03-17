#include "hexaos.h"
#include <esp_system.h>

const char* EspResetReasonText(uint32_t reason) {
  switch ((esp_reset_reason_t)reason) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    default:                return "UNKNOWN";
  }
}

void BootInit() {
}

void BootPrintBanner() {
  LogRaw("========================================");
  LogInfo("%s boot start", HX_NAME);
  LogInfo("Version: %s", HX_VERSION);
  LogInfo("Board:   %s", HX_TARGET_NAME);
  LogRaw("========================================");
}

void BootPrintResetInfo() {
  uint32_t reason = (uint32_t)esp_reset_reason();
  LogInfo("Reset reason: %s (%lu)", EspResetReasonText(reason), (unsigned long)reason);
}
