/*
  HexaOS - factory_data.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Factory data service bootstrap.
  Initializes access to the dedicated factory data storage area intended for immutable manufacturing information and device identity material.
*/

#include "hexaos.h"

bool FactoryDataInit() {
  LogInfo("FACT: init");
  return EspNvsOpenFactory();
}