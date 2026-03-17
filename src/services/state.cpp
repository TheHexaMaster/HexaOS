#include "hexaos.h"
#include "headers/hx_platform_nvs.h"

static bool g_state_ready = false;

bool StateInit() {
  g_state_ready = EspNvsOpenState();
  return g_state_ready;
}

bool StateLoad() {
  if (!g_state_ready) {
    Hx.state_loaded = false;
    return false;
  }

  Hx.state_loaded = true;
  Hx.boot_count = StateGetInt(HX_STATE_BOOT_COUNT, 0) + 1;

  if (!StateSetInt(HX_STATE_BOOT_COUNT, (int32_t)Hx.boot_count)) {
    LogWarn("STA: boot_count store failed");
  }

  return true;
}

bool StateSave() {
  if (!g_state_ready) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_STATE);
}

bool StateGetBool(const char* key, bool defval) {
  bool value = defval;
  if (!g_state_ready) {
    return defval;
  }

  if (HxNvsGetBool(HX_NVS_STORE_STATE, key, &value)) {
    return value;
  }

  return defval;
}

int32_t StateGetInt(const char* key, int32_t defval) {
  int32_t value = defval;
  if (!g_state_ready) {
    return defval;
  }

  if (HxNvsGetInt(HX_NVS_STORE_STATE, key, &value)) {
    return value;
  }

  return defval;
}

bool StateSetBool(const char* key, bool value) {
  if (!g_state_ready) {
    return false;
  }

  if (!HxNvsSetBool(HX_NVS_STORE_STATE, key, value)) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_STATE);
}

bool StateSetInt(const char* key, int32_t value) {
  if (!g_state_ready) {
    return false;
  }

  if (!HxNvsSetInt(HX_NVS_STORE_STATE, key, value)) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_STATE);
}
