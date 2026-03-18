/*
  HexaOS - hexaos.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Primary public system header for HexaOS.
  Aggregates common includes, shared runtime structures and the public prototypes used across core, services, platform adapters and modules.
*/

#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "headers/hx_build.h"
#include "headers/hx_types.h"
#include "headers/hx_module.h"
#include "headers/hx_config_keys.h"
#include "headers/hx_config.h"

struct HxRuntime {
  bool safeboot;
  bool config_loaded;
  bool state_loaded;
  bool littlefs_mounted;
  uint32_t uptime_ms;
  uint32_t boot_count;
};

extern HxRuntime Hx;

// core
void BootInit();
void BootPrintBanner();
void BootPrintResetInfo();

void LogInit();
void LogSetLevel(HxLogLevel level);
HxLogLevel LogGetLevel();
void LogRaw(const char* text);
void LogDebug(const char* fmt, ...);
void LogInfo(const char* fmt, ...);
void LogWarn(const char* fmt, ...);
void LogError(const char* fmt, ...);
void LogTagged(HxLogLevel level, const char* tag, const char* fmt, ...);
size_t LogHistorySize();
size_t LogHistoryCapacity();
size_t LogHistoryCopy(char* out, size_t out_size);
void LogHistoryClear();
uint32_t LogDroppedLines();
uint32_t LogDroppedIsr();
void LogSinkWriteRaw(const char* text);
void LogSinkWriteChar(char ch);
void LogSinkWriteLineRaw(const char* text);
void ConsoleShowPrompt();
void ConsoleOnSinkLockedPreWriteLine();
void ConsoleOnSinkLockedPostWriteLine();

#define HX_LOGD(tag, fmt, ...) LogTagged(HX_LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#define HX_LOGI(tag, fmt, ...) LogTagged(HX_LOG_INFO,  tag, fmt, ##__VA_ARGS__)
#define HX_LOGW(tag, fmt, ...) LogTagged(HX_LOG_WARN,  tag, fmt, ##__VA_ARGS__)
#define HX_LOGE(tag, fmt, ...) LogTagged(HX_LOG_ERROR, tag, fmt, ##__VA_ARGS__)

void Panic(const char* reason);

// platform
void EspPrintChipInfo();
const char* EspResetReasonText(uint32_t reason);

// services
bool FactoryDataInit();

bool StateInit();
bool StateLoad();
bool StateSave();
bool StateGetBool(const char* key, bool defval);
int32_t StateGetInt(const char* key, int32_t defval);
bool StateSetBool(const char* key, bool value);
bool StateSetInt(const char* key, int32_t value);

bool FilesInit();
bool FilesMount();
bool FilesExists(const char* path);
String FilesReadText(const char* path);
bool FilesWriteText(const char* path, const char* text);

// module system
void ModuleInitAll();
void ModuleStartAll();
void ModuleLoopAll();
void ModuleEvery100ms();
void ModuleEverySecond();