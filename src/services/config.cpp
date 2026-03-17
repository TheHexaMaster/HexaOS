#include "hexaos.h"
#include "headers/hx_platform_nvs.h"

static bool g_config_ready = false;

bool ConfigInit() {
  g_config_ready = EspNvsOpenConfig();
  return g_config_ready;
}

bool ConfigLoad() {
  Hx.config_loaded = g_config_ready;
  return g_config_ready;
}

bool ConfigSave() {
  if (!g_config_ready) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_CONFIG);
}

bool ConfigGetBool(const char* key, bool defval) {
  bool value = defval;
  if (!g_config_ready) {
    return defval;
  }

  if (HxNvsGetBool(HX_NVS_STORE_CONFIG, key, &value)) {
    return value;
  }

  return defval;
}

int32_t ConfigGetInt(const char* key, int32_t defval) {
  int32_t value = defval;
  if (!g_config_ready) {
    return defval;
  }

  if (HxNvsGetInt(HX_NVS_STORE_CONFIG, key, &value)) {
    return value;
  }

  return defval;
}

String ConfigGetString(const char* key, const char* defval) {
  if (!g_config_ready) {
    return String(defval ? defval : "");
  }

  String value;
  if (HxNvsGetString(HX_NVS_STORE_CONFIG, key, value)) {
    return value;
  }

  return String(defval ? defval : "");
}

bool ConfigSetBool(const char* key, bool value) {
  if (!g_config_ready) {
    return false;
  }

  if (!HxNvsSetBool(HX_NVS_STORE_CONFIG, key, value)) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_CONFIG);
}

bool ConfigSetInt(const char* key, int32_t value) {
  if (!g_config_ready) {
    return false;
  }

  if (!HxNvsSetInt(HX_NVS_STORE_CONFIG, key, value)) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_CONFIG);
}

bool ConfigSetString(const char* key, const char* value) {
  if (!g_config_ready) {
    return false;
  }

  if (!HxNvsSetString(HX_NVS_STORE_CONFIG, key, value ? value : "")) {
    return false;
  }

  return HxNvsCommit(HX_NVS_STORE_CONFIG);
}
