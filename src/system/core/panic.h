/*
  HexaOS - panic.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Fatal panic handler.
  Provides the last-resort stop path for unrecoverable startup or runtime failures and keeps the system halted after logging the panic reason.
*/

#pragma once

void Panic(const char* reason);
