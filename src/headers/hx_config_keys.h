/*
  HexaOS - hx_config_keys.h

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Central key namespace for persisted configuration and state.
  Defines the canonical string keys used by NVS-backed config and state services to avoid duplicated literals across the codebase.
*/

#pragma once

#define HX_CFG_DEVICE_NAME        "device.name"
#define HX_CFG_LOG_LEVEL          "log.level"
#define HX_CFG_SAFEBOOT_ENABLE    "safeboot.enable"

#define HX_STATE_BOOT_COUNT       "sys.boot_count"
#define HX_STATE_LAST_RESET       "sys.last_reset"