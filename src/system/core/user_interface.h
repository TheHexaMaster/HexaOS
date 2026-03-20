/*
  HexaOS - user_interface.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Core user interface runtime bridge for HexaOS.
  Exposes the mandatory control-plane entry points used by the core boot and
  main loop to initialize the local interactive transport, attach the shared
  shell handler and poll incoming user input outside the optional module layer.
*/

#pragma once

bool UserInterfaceInit();
void UserInterfaceStart();
void UserInterfaceLoop();
