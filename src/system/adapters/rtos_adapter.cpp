/*
  HexaOS - rtos_adapter.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  FreeRTOS backend adapter for HexaOS.
  Owns the concrete FreeRTOS primitives allocated for HexaOS RTOS objects and translates the core RTOS API into ESP32-specific synchronization and timing calls.
*/

#include "rtos_adapter.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdlib.h>

struct HxRtosAdapterCriticalImpl {
  portMUX_TYPE mux;
};

struct HxRtosAdapterMutexImpl {
  StaticSemaphore_t storage;
  SemaphoreHandle_t handle;
};

static TickType_t ClampMsToTicks(uint32_t ms) {
  if (ms == HX_RTOS_WAIT_FOREVER) {
    return portMAX_DELAY;
  }

  return pdMS_TO_TICKS(ms);
}

bool RtosAdapterCriticalInit(HxRtosCritical* critical) {
  if (!critical) {
    return false;
  }

  if (critical->impl) {
    return true;
  }

  HxRtosAdapterCriticalImpl* impl = static_cast<HxRtosAdapterCriticalImpl*>(malloc(sizeof(HxRtosAdapterCriticalImpl)));
  if (!impl) {
    return false;
  }

  impl->mux = portMUX_INITIALIZER_UNLOCKED;
  critical->impl = impl;
  return true;
}

void RtosAdapterCriticalDestroy(HxRtosCritical* critical) {
  if (!critical || !critical->impl) {
    return;
  }

  free(critical->impl);
  critical->impl = nullptr;
}

void RtosAdapterCriticalEnter(HxRtosCritical* critical) {
  if (!critical || !critical->impl) {
    return;
  }

  HxRtosAdapterCriticalImpl* impl = static_cast<HxRtosAdapterCriticalImpl*>(critical->impl);
  if (RtosAdapterInIsr()) {
    taskENTER_CRITICAL_ISR(&impl->mux);
    return;
  }

  taskENTER_CRITICAL(&impl->mux);
}

void RtosAdapterCriticalExit(HxRtosCritical* critical) {
  if (!critical || !critical->impl) {
    return;
  }

  HxRtosAdapterCriticalImpl* impl = static_cast<HxRtosAdapterCriticalImpl*>(critical->impl);
  if (RtosAdapterInIsr()) {
    taskEXIT_CRITICAL_ISR(&impl->mux);
    return;
  }

  taskEXIT_CRITICAL(&impl->mux);
}

bool RtosAdapterMutexInit(HxRtosMutex* mutex) {
  if (!mutex) {
    return false;
  }

  if (mutex->impl) {
    return true;
  }

  HxRtosAdapterMutexImpl* impl = static_cast<HxRtosAdapterMutexImpl*>(malloc(sizeof(HxRtosAdapterMutexImpl)));
  if (!impl) {
    return false;
  }

  impl->handle = xSemaphoreCreateMutexStatic(&impl->storage);
  if (!impl->handle) {
    free(impl);
    return false;
  }

  mutex->impl = impl;
  return true;
}

void RtosAdapterMutexDestroy(HxRtosMutex* mutex) {
  if (!mutex || !mutex->impl) {
    return;
  }

  HxRtosAdapterMutexImpl* impl = static_cast<HxRtosAdapterMutexImpl*>(mutex->impl);
  if (impl->handle) {
    vSemaphoreDelete(impl->handle);
    impl->handle = nullptr;
  }

  free(impl);
  mutex->impl = nullptr;
}

bool RtosAdapterMutexLock(HxRtosMutex* mutex, uint32_t timeout_ms) {
  if (!mutex || !mutex->impl || RtosAdapterInIsr()) {
    return false;
  }

  HxRtosAdapterMutexImpl* impl = static_cast<HxRtosAdapterMutexImpl*>(mutex->impl);
  if (!impl->handle) {
    return false;
  }

  return (xSemaphoreTake(impl->handle, ClampMsToTicks(timeout_ms)) == pdTRUE);
}

void RtosAdapterMutexUnlock(HxRtosMutex* mutex) {
  if (!mutex || !mutex->impl || RtosAdapterInIsr()) {
    return;
  }

  HxRtosAdapterMutexImpl* impl = static_cast<HxRtosAdapterMutexImpl*>(mutex->impl);
  if (!impl->handle) {
    return;
  }

  xSemaphoreGive(impl->handle);
}

void RtosAdapterSleepMs(uint32_t ms) {
  if (RtosAdapterInIsr()) {
    return;
  }

  if (ms == 0) {
    taskYIELD();
    return;
  }

  vTaskDelay(ClampMsToTicks(ms));
}

void RtosAdapterYield() {
  if (RtosAdapterInIsr()) {
    return;
  }

  taskYIELD();
}

bool RtosAdapterInIsr() {
  return xPortInIsrContext();
}

uint32_t RtosAdapterTickCount() {
  return static_cast<uint32_t>(xTaskGetTickCount());
}

uint32_t RtosAdapterMsToTicks(uint32_t ms) {
  return static_cast<uint32_t>(ClampMsToTicks(ms));
}
