/*
  HexaOS - sdmmc_adapter.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Unified SDMMC + FatFS adapter implementation.

  Pin resolution order:
    1. SDMMC0 native pins  (HX_PIN_SDMMC0_CLK/CMD/D0..D3)
    2. HOSTED0 SDIO pins   (HX_PIN_HOSTED0_SDIO_CLK/CMD/D0..D3) as fallback
       — HX_PIN_HOSTED0_RESET and HX_PIN_HOSTED0_BOOT are optional and
         are logged when mapped but not required for operation.

  Bus width:
    - CLK, CMD, D0 mandatory.
    - D1 + D2 + D3 all mapped → 4-bit mode.
    - Any D1-D3 missing → 1-bit mode (incomplete 4-bit config is warned).

  Power sequencing (both applied before esp_vfs_fat_sdmmc_mount):
    - On-chip LDO (SOC_SDMMC_IO_POWER_EXTERNAL, e.g. ESP32-P4): enabled via
      sd_pwr_ctrl_new_on_chip_ldo() using HX_SDMMC_POWER_CHANNEL.
      Picks up BOARD_SDMMC_POWER_CHANNEL from pins_arduino.h when present.
    - GPIO power switch (HX_PIN_SDMMC0_POWER in pinmap): toggled to
      HX_SDMMC_POWER_ON_LEVEL after a 10 ms off-pulse, 50 ms on-settle.
      Picks up BOARD_SDMMC_POWER_ON_LEVEL from pins_arduino.h when present.
  SDMMC host slot: HX_SDMMC_SLOT (default 0, picks up BOARD_SDMMC_SLOT).

  On successful mount, sets Hx.sd_mounted = true.
  On unmount, clears Hx.sd_mounted.
  Init and mount failures produce log errors but never trigger panic.
*/

#include "sdmmc_adapter.h"

#include "headers/hx_build.h"

#if HX_ENABLE_FEATURE_SD

#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef SOC_SDMMC_IO_POWER_EXTERNAL
  #include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "headers/hx_pinfunc.h"
#include "system/core/log.h"
#include "system/core/pinmap.h"
#include "system/core/runtime.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr const char* HX_SDMMC_TAG   = "SDMMC";
static constexpr const char* SDMMC_MOUNT_PT = "/sd";
static constexpr size_t      SDMMC_PATH_MAX = 512;

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static sdmmc_card_t* g_card    = nullptr;
static bool          g_mounted = false;

// Resolved pin configuration — filled by SdmmcInit.
static int  g_pin_clk   = -1;
static int  g_pin_cmd   = -1;
static int  g_pin_d0    = -1;
static int  g_pin_d1    = -1;
static int  g_pin_d2    = -1;
static int  g_pin_d3    = -1;
static int  g_pin_power = -1;  // Optional GPIO power switch; -1 = not mapped.
static int  g_bus_width = 0;   // 0 = not initialised, 1 or 4

#ifdef SOC_SDMMC_IO_POWER_EXTERNAL
static sd_pwr_ctrl_handle_t g_pwr_ctrl_handle = nullptr;
#endif

// ---------------------------------------------------------------------------
// Internal: path helpers
// ---------------------------------------------------------------------------

// Prepend SDMMC_MOUNT_PT to a handler-relative path.
// path must start with '/'. Result: /sd + path (e.g. /sd/config.json).
static bool SdmmcBuildPath(const char* path, char* out, size_t out_size) {
  int n = snprintf(out, out_size, "%s%s", SDMMC_MOUNT_PT, path);
  return (n > 0 && (size_t)n < out_size);
}

// ---------------------------------------------------------------------------
// Internal: pin resolution
// ---------------------------------------------------------------------------

// Resolve bus width from three optional data lines.
// All three must be mapped for 4-bit; otherwise 1-bit.
static int SdmmcResolveWidth(int d1, int d2, int d3) {
  if (d1 >= 0 && d2 >= 0 && d3 >= 0) {
    return 4;
  }
  if (d1 >= 0 || d2 >= 0 || d3 >= 0) {
    HX_LOGW(HX_SDMMC_TAG,
            "incomplete 4-bit config (d1=%d d2=%d d3=%d) — using 1-bit mode",
            d1, d2, d3);
  }
  return 1;
}

// Resolve the optional power pin shared across both SDMMC0 and HOSTED0 pin sets.
static void SdmmcResolvePowerPin() {
  g_pin_power = PinmapGetGpioForFunction(HX_PIN_SDMMC0_POWER);
  if (g_pin_power >= 0) {
    HX_LOGI(HX_SDMMC_TAG, "power pin gpio=%d active-level=%d",
            g_pin_power, HX_SDMMC_POWER_ON_LEVEL);
  }
}

static bool SdmmcResolvePins() {
  g_bus_width = 0;

  // ---- Try SDMMC0 native pins first ----------------------------------------
  {
    int clk = PinmapGetGpioForFunction(HX_PIN_SDMMC0_CLK);
    int cmd = PinmapGetGpioForFunction(HX_PIN_SDMMC0_CMD);
    int d0  = PinmapGetGpioForFunction(HX_PIN_SDMMC0_D0);

    if (clk >= 0 && cmd >= 0 && d0 >= 0) {
      int d1 = PinmapGetGpioForFunction(HX_PIN_SDMMC0_D1);
      int d2 = PinmapGetGpioForFunction(HX_PIN_SDMMC0_D2);
      int d3 = PinmapGetGpioForFunction(HX_PIN_SDMMC0_D3);
      int w  = SdmmcResolveWidth(d1, d2, d3);

      g_pin_clk   = clk;
      g_pin_cmd   = cmd;
      g_pin_d0    = d0;
      g_pin_d1    = (w == 4) ? d1 : -1;
      g_pin_d2    = (w == 4) ? d2 : -1;
      g_pin_d3    = (w == 4) ? d3 : -1;
      g_bus_width = w;

      HX_LOGI(HX_SDMMC_TAG,
              "pins: SDMMC0 clk=%d cmd=%d d0=%d d1=%d d2=%d d3=%d width=%d-bit",
              clk, cmd, d0, d1, d2, d3, w);
      SdmmcResolvePowerPin();
      return true;
    }
  }

  // ---- Fallback: HOSTED0 SDIO pins -----------------------------------------
  {
    int clk = PinmapGetGpioForFunction(HX_PIN_HOSTED0_SDIO_CLK);
    int cmd = PinmapGetGpioForFunction(HX_PIN_HOSTED0_SDIO_CMD);
    int d0  = PinmapGetGpioForFunction(HX_PIN_HOSTED0_SDIO_D0);

    if (clk >= 0 && cmd >= 0 && d0 >= 0) {
      int d1 = PinmapGetGpioForFunction(HX_PIN_HOSTED0_SDIO_D1);
      int d2 = PinmapGetGpioForFunction(HX_PIN_HOSTED0_SDIO_D2);
      int d3 = PinmapGetGpioForFunction(HX_PIN_HOSTED0_SDIO_D3);
      int w  = SdmmcResolveWidth(d1, d2, d3);

      g_pin_clk   = clk;
      g_pin_cmd   = cmd;
      g_pin_d0    = d0;
      g_pin_d1    = (w == 4) ? d1 : -1;
      g_pin_d2    = (w == 4) ? d2 : -1;
      g_pin_d3    = (w == 4) ? d3 : -1;
      g_bus_width = w;

      // RESET and BOOT are optional control lines — log if mapped, do not require.
      int rst  = PinmapGetGpioForFunction(HX_PIN_HOSTED0_RESET);
      int boot = PinmapGetGpioForFunction(HX_PIN_HOSTED0_BOOT);
      if (rst  >= 0) { HX_LOGI(HX_SDMMC_TAG, "HOSTED0 RESET gpio=%d (optional)", rst);  }
      if (boot >= 0) { HX_LOGI(HX_SDMMC_TAG, "HOSTED0 BOOT  gpio=%d (optional)", boot); }

      HX_LOGI(HX_SDMMC_TAG,
              "pins: HOSTED0 SDIO clk=%d cmd=%d d0=%d d1=%d d2=%d d3=%d width=%d-bit",
              clk, cmd, d0, d1, d2, d3, w);
      SdmmcResolvePowerPin();
      return true;
    }
  }

  HX_LOGE(HX_SDMMC_TAG, "no SDMMC pin set found (SDMMC0 or HOSTED0 SDIO)");
  return false;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool SdmmcInit() {
  return SdmmcResolvePins();
}

bool SdmmcMount() {
  if (g_mounted) {
    return true;
  }

  if (g_bus_width == 0) {
    HX_LOGE(HX_SDMMC_TAG, "mount called before SdmmcInit");
    return false;
  }

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.slot = HX_SDMMC_SLOT;

  // On-chip LDO power channel — ESP32-P4 and other chips with SOC_SDMMC_IO_POWER_EXTERNAL.
  // Enables the SDMMC IO power rail through the chip's internal LDO controller.
#ifdef SOC_SDMMC_IO_POWER_EXTERNAL
  if (HX_SDMMC_POWER_CHANNEL >= 0) {
    sd_pwr_ctrl_ldo_config_t ldo_cfg = {
      .ldo_chan_id = HX_SDMMC_POWER_CHANNEL,
    };
    esp_err_t ldo_err = sd_pwr_ctrl_new_on_chip_ldo(&ldo_cfg, &g_pwr_ctrl_handle);
    if (ldo_err != ESP_OK) {
      HX_LOGE(HX_SDMMC_TAG, "LDO power channel=%d init failed err=%d",
              HX_SDMMC_POWER_CHANNEL, (int)ldo_err);
      return false;
    }
    host.pwr_ctrl_handle = g_pwr_ctrl_handle;
    HX_LOGI(HX_SDMMC_TAG, "LDO power channel=%d enabled", HX_SDMMC_POWER_CHANNEL);
  }
#endif

  // External GPIO power switch — cycle off then on to ensure a clean power-up.
  if (g_pin_power >= 0) {
    gpio_reset_pin((gpio_num_t)g_pin_power);
    gpio_set_direction((gpio_num_t)g_pin_power, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)g_pin_power, HX_SDMMC_POWER_ON_LEVEL ? 0 : 1);  // deassert
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)g_pin_power, HX_SDMMC_POWER_ON_LEVEL);           // assert
    vTaskDelay(pdMS_TO_TICKS(50));  // allow card to complete power-up
    HX_LOGI(HX_SDMMC_TAG, "power pin gpio=%d asserted (level=%d)",
            g_pin_power, HX_SDMMC_POWER_ON_LEVEL);
  }

  sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
  slot.clk   = (gpio_num_t)g_pin_clk;
  slot.cmd   = (gpio_num_t)g_pin_cmd;
  slot.d0    = (gpio_num_t)g_pin_d0;
  slot.cd    = SDMMC_SLOT_NO_CD;
  slot.wp    = SDMMC_SLOT_NO_WP;
  slot.width = (uint8_t)g_bus_width;
  // Enable internal pull-ups on all active SDMMC lines. Weak (~50 kΩ) but
  // sufficient for short traces. For longer traces use external 10 kΩ pull-ups.
  slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  if (g_bus_width == 4) {
    slot.d1 = (gpio_num_t)g_pin_d1;
    slot.d2 = (gpio_num_t)g_pin_d2;
    slot.d3 = (gpio_num_t)g_pin_d3;
  } else {
    slot.d1 = GPIO_NUM_NC;
    slot.d2 = GPIO_NUM_NC;
    slot.d3 = GPIO_NUM_NC;
  }

  // D4-D7 (8-bit MMC) not supported; left at GPIO_NUM_NC from SDMMC_SLOT_CONFIG_DEFAULT.

  esp_vfs_fat_mount_config_t mount_cfg = {};
  mount_cfg.format_if_mount_failed = false;
  mount_cfg.max_files              = 8;
  mount_cfg.allocation_unit_size   = 16 * 1024;

  esp_err_t err = esp_vfs_fat_sdmmc_mount(SDMMC_MOUNT_PT, &host, &slot, &mount_cfg, &g_card);
  if (err != ESP_OK) {
    HX_LOGE(HX_SDMMC_TAG, "mount failed at %s err=%d width=%d-bit",
            SDMMC_MOUNT_PT, (int)err, g_bus_width);
    g_card = nullptr;
    return false;
  }

  g_mounted        = true;
  Hx.sd_mounted    = true;

  uint64_t cap_mb = ((uint64_t)(uint32_t)g_card->csd.capacity
                     * (uint32_t)g_card->csd.sector_size)
                    / (1024ULL * 1024ULL);

  HX_LOGI(HX_SDMMC_TAG, "mount OK at %s width=%d-bit capacity=%lluMB speed=%lukHz",
          SDMMC_MOUNT_PT,
          g_bus_width,
          (unsigned long long)cap_mb,
          (unsigned long)g_card->real_freq_khz);

  return true;
}

bool SdmmcUnmount() {
  if (!g_mounted || !g_card) {
    return true;
  }

  esp_err_t err = esp_vfs_fat_sdcard_unmount(SDMMC_MOUNT_PT, g_card);
  g_card        = nullptr;
  g_mounted     = false;
  Hx.sd_mounted = false;

#ifdef SOC_SDMMC_IO_POWER_EXTERNAL
  if (g_pwr_ctrl_handle != nullptr) {
    sd_pwr_ctrl_del_on_chip_ldo(g_pwr_ctrl_handle);
    g_pwr_ctrl_handle = nullptr;
  }
#endif

  if (g_pin_power >= 0) {
    gpio_set_level((gpio_num_t)g_pin_power, HX_SDMMC_POWER_ON_LEVEL ? 0 : 1);  // deassert
  }

  if (err != ESP_OK) {
    HX_LOGE(HX_SDMMC_TAG, "unmount failed err=%d", (int)err);
    return false;
  }

  HX_LOGI(HX_SDMMC_TAG, "unmount OK");
  return true;
}

bool SdmmcFormat() {
  // esp_vfs_fat_sdcard_format is available in ESP-IDF 5.x+. Not implemented.
  HX_LOGW(HX_SDMMC_TAG, "format not supported");
  return false;
}

// ---------------------------------------------------------------------------
// File and directory operations (POSIX through VFS after mount)
// ---------------------------------------------------------------------------

bool SdmmcExists(const char* path) {
  char full[SDMMC_PATH_MAX];
  if (!SdmmcBuildPath(path, full, sizeof(full))) {
    return false;
  }
  struct stat st;
  return (stat(full, &st) == 0);
}

bool SdmmcRemove(const char* path) {
  char full[SDMMC_PATH_MAX];
  if (!SdmmcBuildPath(path, full, sizeof(full))) {
    return false;
  }
  return (unlink(full) == 0);
}

bool SdmmcRename(const char* old_path, const char* new_path) {
  char old_full[SDMMC_PATH_MAX];
  char new_full[SDMMC_PATH_MAX];
  if (!SdmmcBuildPath(old_path, old_full, sizeof(old_full))) {
    return false;
  }
  if (!SdmmcBuildPath(new_path, new_full, sizeof(new_full))) {
    return false;
  }
  return (rename(old_full, new_full) == 0);
}

bool SdmmcMkdir(const char* path) {
  char full[SDMMC_PATH_MAX];
  if (!SdmmcBuildPath(path, full, sizeof(full))) {
    return false;
  }
  return (mkdir(full, 0755) == 0);
}

bool SdmmcRmdir(const char* path) {
  char full[SDMMC_PATH_MAX];
  if (!SdmmcBuildPath(path, full, sizeof(full))) {
    return false;
  }
  return (rmdir(full) == 0);
}

bool SdmmcStat(const char* path, bool* out_is_dir, size_t* out_size) {
  char full[SDMMC_PATH_MAX];
  if (!SdmmcBuildPath(path, full, sizeof(full))) {
    return false;
  }
  struct stat st;
  if (stat(full, &st) != 0) {
    return false;
  }
  bool is_dir = S_ISDIR(st.st_mode);
  if (out_is_dir) { *out_is_dir = is_dir; }
  if (out_size)   { *out_size   = is_dir ? 0 : (size_t)st.st_size; }
  return true;
}

bool SdmmcGetStorageInfo(size_t* out_total, size_t* out_used) {
  struct statvfs sv;
  if (statvfs(SDMMC_MOUNT_PT, &sv) != 0) {
    return false;
  }
  uint64_t total  = (uint64_t)sv.f_bsize * (uint64_t)sv.f_blocks;
  uint64_t free_b = (uint64_t)sv.f_bsize * (uint64_t)sv.f_bfree;
  if (out_total) { *out_total = (size_t)total; }
  if (out_used)  { *out_used  = (size_t)(total > free_b ? total - free_b : 0); }
  return true;
}

bool SdmmcList(const char* path, SdmmcListCallback callback, void* user) {
  if (!callback) {
    return false;
  }

  char full_dir[SDMMC_PATH_MAX];
  if (!SdmmcBuildPath(path, full_dir, sizeof(full_dir))) {
    return false;
  }

  DIR* dir = opendir(full_dir);
  if (!dir) {
    return false;
  }

  size_t path_len  = strlen(path);
  bool   has_slash = (path_len > 0 && path[path_len - 1] == '/');

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char entry_path[SDMMC_PATH_MAX];
    int n = has_slash
              ? snprintf(entry_path, sizeof(entry_path), "%s%s",  path, entry->d_name)
              : snprintf(entry_path, sizeof(entry_path), "%s/%s", path, entry->d_name);
    if (n <= 0 || (size_t)n >= sizeof(entry_path)) {
      continue;
    }

    bool   is_dir     = (entry->d_type == DT_DIR);
    size_t size_bytes = 0;

    if (!is_dir) {
      char full_entry[SDMMC_PATH_MAX];
      if (SdmmcBuildPath(entry_path, full_entry, sizeof(full_entry))) {
        struct stat st;
        if (stat(full_entry, &st) == 0) {
          size_bytes = (size_t)st.st_size;
        }
      }
    }

    if (!callback(entry_path, is_dir, size_bytes, user)) {
      closedir(dir);
      return true;
    }
  }

  closedir(dir);
  return true;
}

bool SdmmcReadBytes(const char* path, uint8_t* out, size_t out_size, size_t* out_len) {
  if (out_len) {
    *out_len = 0;
  }

  char full[SDMMC_PATH_MAX];
  if (!SdmmcBuildPath(path, full, sizeof(full))) {
    return false;
  }

  FILE* f = fopen(full, "rb");
  if (!f) {
    return false;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  long file_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (file_size < 0 || (size_t)file_size > out_size) {
    fclose(f);
    return false;
  }

  size_t to_read  = (size_t)file_size;
  size_t read_len = (to_read > 0) ? fread(out, 1, to_read, f) : 0;
  fclose(f);

  if (out_len) { *out_len = read_len; }
  return (read_len == to_read);
}

bool SdmmcWriteBytes(const char* path, const uint8_t* data, size_t len, bool append) {
  char full[SDMMC_PATH_MAX];
  if (!SdmmcBuildPath(path, full, sizeof(full))) {
    return false;
  }

  FILE* f = fopen(full, append ? "ab" : "wb");
  if (!f) {
    return false;
  }

  if (len > 0 && data) {
    size_t written = fwrite(data, 1, len, f);
    if (written != len) {
      fclose(f);
      return false;
    }
  }

  fclose(f);
  return true;
}

#endif // HX_ENABLE_FEATURE_SD
