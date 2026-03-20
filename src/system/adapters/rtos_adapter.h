/*
  HexaOS - rtos_adapter.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Low-level RTOS backend adapter for HexaOS.
  Maps the handler-level RTOS primitives to the concrete FreeRTOS/ESP platform implementation.
*/

#pragma once

#include "headers/hx_rtos.h"

bool RtosAdapterCriticalInit(HxRtosCritical* critical);
void RtosAdapterCriticalDestroy(HxRtosCritical* critical);
void RtosAdapterCriticalEnter(HxRtosCritical* critical);
void RtosAdapterCriticalExit(HxRtosCritical* critical);

bool RtosAdapterMutexInit(HxRtosMutex* mutex);
void RtosAdapterMutexDestroy(HxRtosMutex* mutex);
bool RtosAdapterMutexLock(HxRtosMutex* mutex, uint32_t timeout_ms);
void RtosAdapterMutexUnlock(HxRtosMutex* mutex);

void RtosAdapterSleepMs(uint32_t ms);
void RtosAdapterYield();
bool RtosAdapterInIsr();
uint32_t RtosAdapterTickCount();
uint32_t RtosAdapterMsToTicks(uint32_t ms);
