/*
  HexaOS - rtos_handler.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Central RTOS service layer for HexaOS.
  Declares the stable synchronization and timing primitives that higher-level modules, handlers and core services should use across the firmware.
*/

#pragma once

#include <stdint.h>

struct HxRtosCritical {
  void* impl;
};

struct HxRtosMutex {
  void* impl;
};

static constexpr uint32_t HX_RTOS_NO_WAIT = 0U;
static constexpr uint32_t HX_RTOS_WAIT_FOREVER = 0xFFFFFFFFu;

#define HX_RTOS_CRITICAL_INIT { nullptr }
#define HX_RTOS_MUTEX_INIT    { nullptr }

bool RtosInit();
bool RtosReady();

bool RtosCriticalInit(HxRtosCritical* critical);
void RtosCriticalDestroy(HxRtosCritical* critical);
bool RtosCriticalReady(const HxRtosCritical* critical);
void RtosCriticalEnter(HxRtosCritical* critical);
void RtosCriticalExit(HxRtosCritical* critical);

bool RtosMutexInit(HxRtosMutex* mutex);
void RtosMutexDestroy(HxRtosMutex* mutex);
bool RtosMutexReady(const HxRtosMutex* mutex);
bool RtosMutexLock(HxRtosMutex* mutex, uint32_t timeout_ms);
bool RtosMutexTryLock(HxRtosMutex* mutex);
void RtosMutexUnlock(HxRtosMutex* mutex);

void RtosSleepMs(uint32_t ms);
void RtosYield();
bool RtosInIsr();
uint32_t RtosTickCount();
uint32_t RtosMsToTicks(uint32_t ms);
