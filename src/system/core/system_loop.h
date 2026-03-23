/*
  HexaOS - system_loop.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Core runtime service loop API for HexaOS.
  Declares the mandatory non-module runtime callbacks executed from the top-
  level firmware loop, including the continuous core service pump and periodic
  system ticks such as the built-in heartbeat message.
*/

#pragma once

void SystemLoop();
void SystemEvery10ms();
void SystemEvery100ms();
void SystemEverySecond();
void HeartBeatTick();
