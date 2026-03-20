/*
  HexaOS - rtos.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Native core RTOS service implementation for HexaOS.
  Provides the central synchronization and timing API exposed to the rest of the system while delegating the low-level backend details to the RTOS adapter layer.
*/

#include "rtos.h"
#include "system/core/runtime.h"
#include "system/adapters/rtos_adapter.h"

static bool g_rtos_ready = false;

static bool EnsureRtosReady() {
  if (g_rtos_ready) {
    return true;
  }

  return RtosInit();
}

bool RtosInit() {
  if (g_rtos_ready) {
    Hx.rtos_ready = true;
    return true;
  }

  g_rtos_ready = true;
  Hx.rtos_ready = true;
  return true;
}

bool RtosReady() {
  return g_rtos_ready;
}

bool RtosCriticalInit(HxRtosCritical* critical) {
  if (!EnsureRtosReady()) {
    return false;
  }

  return RtosAdapterCriticalInit(critical);
}

void RtosCriticalDestroy(HxRtosCritical* critical) {
  RtosAdapterCriticalDestroy(critical);
}

bool RtosCriticalReady(const HxRtosCritical* critical) {
  return (critical && (critical->impl != nullptr));
}

void RtosCriticalEnter(HxRtosCritical* critical) {
  if (!EnsureRtosReady()) {
    return;
  }

  RtosAdapterCriticalEnter(critical);
}

void RtosCriticalExit(HxRtosCritical* critical) {
  if (!EnsureRtosReady()) {
    return;
  }

  RtosAdapterCriticalExit(critical);
}

bool RtosMutexInit(HxRtosMutex* mutex) {
  if (!EnsureRtosReady()) {
    return false;
  }

  return RtosAdapterMutexInit(mutex);
}

void RtosMutexDestroy(HxRtosMutex* mutex) {
  RtosAdapterMutexDestroy(mutex);
}

bool RtosMutexReady(const HxRtosMutex* mutex) {
  return (mutex && (mutex->impl != nullptr));
}

bool RtosMutexLock(HxRtosMutex* mutex, uint32_t timeout_ms) {
  if (!EnsureRtosReady()) {
    return false;
  }

  return RtosAdapterMutexLock(mutex, timeout_ms);
}

bool RtosMutexTryLock(HxRtosMutex* mutex) {
  return RtosMutexLock(mutex, HX_RTOS_NO_WAIT);
}

void RtosMutexUnlock(HxRtosMutex* mutex) {
  if (!EnsureRtosReady()) {
    return;
  }

  RtosAdapterMutexUnlock(mutex);
}

void RtosSleepMs(uint32_t ms) {
  if (!EnsureRtosReady()) {
    return;
  }

  RtosAdapterSleepMs(ms);
}

void RtosYield() {
  if (!EnsureRtosReady()) {
    return;
  }

  RtosAdapterYield();
}

bool RtosInIsr() {
  if (!EnsureRtosReady()) {
    return false;
  }

  return RtosAdapterInIsr();
}

uint32_t RtosTickCount() {
  if (!EnsureRtosReady()) {
    return 0;
  }

  return RtosAdapterTickCount();
}

uint32_t RtosMsToTicks(uint32_t ms) {
  if (!EnsureRtosReady()) {
    return 0;
  }

  return RtosAdapterMsToTicks(ms);
}
