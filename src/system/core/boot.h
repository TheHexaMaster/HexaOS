/*
  HexaOS - boot.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Core boot orchestration API for HexaOS.
  Declares the single boot entry point responsible for initializing mandatory
  core services, loading persistent configuration and state, mounting required
  storage backends and starting the built-in user interface before optional
  modules are started by the top-level firmware entrypoint.
*/

#pragma once

void BootInit();
