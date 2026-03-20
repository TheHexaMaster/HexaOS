/*
  HexaOS - user_interface.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Mandatory core user interface runtime bridge for HexaOS.
  Initializes the active local control-plane transport selected at build time,
  registers it as the active log sink, wires it into the shared user interface
  handler and polls incoming user input directly from the core runtime loop.
*/

#include "system/core/user_interface.h"
#include "system/core/log.h"
#include "system/adapters/console_adapter.h"
#include "system/handlers/user_interface_handler.h"

static HxLogSinkWriteOps g_user_interface_log_sink_ops = {
  .write_data = ConsoleAdapterWriteData,
  .write_text = ConsoleAdapterWriteText,
  .write_char = ConsoleAdapterWriteChar,
  .flush = ConsoleAdapterFlush
};

static HxUserInterfaceWriteOps g_user_interface_write_ops = {
  .write_data = ConsoleAdapterWriteData,
  .write_text = ConsoleAdapterWriteText,
  .write_char = ConsoleAdapterWriteChar,
  .flush = ConsoleAdapterFlush
};

static bool g_user_interface_ready = false;

bool UserInterfaceInit() {
  if (g_user_interface_ready) {
    return true;
  }

  if (!ConsoleAdapterInit()) {
    HX_LOGE("UI", "transport init failed");
    return false;
  }

  LogSetSinkWriteOps(&g_user_interface_log_sink_ops);

  if (!UserInterfaceHandlerInit(&g_user_interface_write_ops)) {
    HX_LOGE("UI", "handler init failed");
    return false;
  }

  g_user_interface_ready = true;
  HX_LOGI("UI", "init");
  return true;
}

void UserInterfaceStart() {
  if (!g_user_interface_ready) {
    return;
  }

  UserInterfaceHandlerStart();
}

void UserInterfaceLoop() {
  if (!g_user_interface_ready) {
    return;
  }

  while (true) {
    int ch = ConsoleAdapterReadByte();
    if (ch < 0) {
      break;
    }

    UserInterfaceHandlerHandleByte(ch);
  }
}
