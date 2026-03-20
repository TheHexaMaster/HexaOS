/*
  HexaOS - user_interface_handler.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Shared user interface shell/session handler for HexaOS.
  Owns prompt state, line editing, command dispatch integration and log-aware
  redraw hooks while remaining transport-agnostic through pluggable write
  callbacks supplied by the core user interface runtime bridge.
*/

#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
  size_t (*write_data)(const uint8_t* data, size_t len);
  size_t (*write_text)(const char* text);
  size_t (*write_char)(char ch);
  void (*flush)();
} HxUserInterfaceWriteOps;

bool UserInterfaceHandlerInit(const HxUserInterfaceWriteOps* ops);
void UserInterfaceHandlerStart();
void UserInterfaceHandlerHandleByte(int ch);
