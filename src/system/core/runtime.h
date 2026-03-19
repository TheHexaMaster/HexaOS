#pragma once

struct HxRuntime {
  bool safeboot;
  bool config_loaded;
  bool state_loaded;
  bool littlefs_mounted;
  uint32_t uptime_ms;
  uint32_t boot_count;
};

extern HxRuntime Hx;