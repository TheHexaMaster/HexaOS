#include "hexaos.h"

HxRuntime Hx = {
  .safeboot = false,
  .config_loaded = false,
  .state_loaded = false,
  .littlefs_mounted = false,
  .uptime_ms = 0,
  .boot_count = 0
};