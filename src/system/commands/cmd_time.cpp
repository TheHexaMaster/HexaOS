/*
  HexaOS - cmd_time.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Time inspection and control commands for HexaOS.
  Registers: time, time status, time setepoch, time clear.
*/

#include <stdio.h>

#include "cmd_parse.h"
#include "command_engine.h"
#include "system/core/time.h"

static void CmdFormatUint64(char* out, size_t out_size, uint64_t value) {
  if (!TimeFormatUint64(out, out_size, value)) {
    if (out && (out_size > 0)) {
      out[0] = '\0';
    }
  }
}

static HxCmdStatus CmdTimeStatus(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: time status");
    return HX_CMD_USAGE;
  }

  HxTimeInfo info{};
  if (!TimeGetInfo(&info)) {
    CmdOutWriteLine(out, "time status failed");
    return HX_CMD_ERROR;
  }

  char monotonic_text[32];
  TimeFormatMonotonic(monotonic_text, sizeof(monotonic_text), info.monotonic_ms);

  CmdOutPrintfLine(out, "ready = %s", info.ready ? "true" : "false");
  CmdOutPrintfLine(out, "synchronized = %s", info.synchronized ? "true" : "false");
  CmdOutPrintfLine(out, "source = %s", TimeSourceText(info.source));
  char monotonic_ms_text[24];
  CmdFormatUint64(monotonic_ms_text, sizeof(monotonic_ms_text), info.monotonic_ms);
  CmdOutPrintfLine(out, "monotonic_ms = %s", monotonic_ms_text);
  CmdOutPrintfLine(out, "uptime = %s", monotonic_text);

  if (!info.synchronized) {
    CmdOutWriteLine(out, "utc = -");
    return HX_CMD_OK;
  }

  char utc_text[40];
  if (!TimeFormatUtc(utc_text, sizeof(utc_text), info.unix_ms)) {
    CmdOutWriteLine(out, "utc = <format-error>");
    return HX_CMD_OK;
  }

  char unix_ms_text[24];
  char unix_s_text[24];
  char sync_age_text[24];
  CmdFormatUint64(unix_ms_text, sizeof(unix_ms_text), info.unix_ms);
  CmdFormatUint64(unix_s_text, sizeof(unix_s_text), info.unix_ms / 1000ULL);
  CmdFormatUint64(sync_age_text, sizeof(sync_age_text), info.sync_age_ms);
  CmdOutPrintfLine(out, "unix_ms = %s", unix_ms_text);
  CmdOutPrintfLine(out, "unix_s = %s", unix_s_text);
  CmdOutPrintfLine(out, "sync_age_ms = %s", sync_age_text);
  CmdOutPrintfLine(out, "utc = %s", utc_text);
  return HX_CMD_OK;
}

static HxCmdStatus CmdTimeSetEpoch(const char* args, HxCmdOutput* out) {
  uint64_t unix_seconds = 0;

  const char* text = CmdSkipWs(args);
  if (text[0] == '\0') {
    CmdOutWriteLine(out, "usage: time setepoch <unix_seconds>");
    return HX_CMD_USAGE;
  }

  char token[32];
  if (!CmdExtractToken(&text, token, sizeof(token)) || (CmdSkipWs(text)[0] != '\0')) {
    CmdOutWriteLine(out, "usage: time setepoch <unix_seconds>");
    return HX_CMD_USAGE;
  }

  char* endptr = nullptr;
  unsigned long long parsed = strtoull(token, &endptr, 10);
  if ((endptr == token) || (*endptr != '\0')) {
    CmdOutWriteLine(out, "invalid unix_seconds");
    return HX_CMD_ERROR;
  }

  unix_seconds = (uint64_t)parsed;
  if (!TimeSetUnixSeconds(unix_seconds, HX_TIME_SOURCE_USER)) {
    CmdOutWriteLine(out, "time set failed");
    return HX_CMD_ERROR;
  }

  char utc_text[40];
  if (!TimeFormatNowUtc(utc_text, sizeof(utc_text))) {
    CmdOutWriteLine(out, "time set but formatting failed");
    return HX_CMD_OK;
  }

  CmdOutPrintfLine(out, "time set to %s", utc_text);
  return HX_CMD_OK;
}

static HxCmdStatus CmdTimeClear(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: time clear");
    return HX_CMD_USAGE;
  }

  TimeClearSynchronization();
  CmdOutWriteLine(out, "time synchronization cleared");
  return HX_CMD_OK;
}

static const HxCmdDef kTimeCommands[] = {
  { "time",          CmdTimeStatus,   "Show current time status" },
  { "time status",   CmdTimeStatus,   nullptr },
  { "time setepoch", CmdTimeSetEpoch, "Set synchronized time from unix seconds" },
  { "time clear",    CmdTimeClear,    "Clear synchronized wall clock" }
};

bool CommandRegisterTime() {
  for (size_t i = 0; i < (sizeof(kTimeCommands) / sizeof(kTimeCommands[0])); i++) {
    if (!CommandRegister(&kTimeCommands[i])) {
      return false;
    }
  }
  return true;
}
