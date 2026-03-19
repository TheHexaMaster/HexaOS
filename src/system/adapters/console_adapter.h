/*
  HexaOS - console_adapter.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Console transport adapter abstraction for HexaOS.
  Provides a small backend-neutral API used by the logger and interactive shell so the active console transport can be selected at build time.
*/

#pragma once

#include <stddef.h>
#include <stdint.h>

bool ConsoleAdapterInit();
int ConsoleAdapterReadByte();
size_t ConsoleAdapterWriteData(const uint8_t* data, size_t len);
size_t ConsoleAdapterWriteText(const char* text);
size_t ConsoleAdapterWriteChar(char ch);
void ConsoleAdapterFlush();
