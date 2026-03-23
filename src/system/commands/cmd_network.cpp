/*
  HexaOS - cmd_network.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Network inspection and control commands for HexaOS.
  Registers: net, net status, net connect, net disconnect, net info,
             net save.
  All operations are dispatched through network_handler.
  Gated by HX_ENABLE_MODULE_NETWORK.
*/

#include <stdio.h>
#include <string.h>

#include "cmd_parse.h"
#include "command_engine.h"
#include "headers/hx_build.h"

#if HX_ENABLE_MODULE_NETWORK

#include "system/core/config.h"
#include "system/handlers/network_handler.h"

// ---------------------------------------------------------------------------
// net / net status
// ---------------------------------------------------------------------------

static HxCmdStatus CmdNetStatus(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: net status");
    return HX_CMD_USAGE;
  }

  HxNetworkState state = NetworkGetState();
  CmdOutWriteLine(out, "network:");
  CmdOutPrintfLine(out, "  wifi       = %s", NetworkStateStr(state));
#if HX_ENABLE_FEATURE_ETH
  CmdOutPrintfLine(out, "  eth        = %s", NetworkEthIsUp() ? "up" : "down");
#endif

  if (NetworkIsConnected()) {
    char ip[32];
    if (NetworkGetIp(ip, sizeof(ip))) {
      CmdOutPrintfLine(out, "  ip         = %s", ip);
    }
    if (state == HX_NETWORK_STATE_CONNECTED) {
      CmdOutPrintfLine(out, "  rssi       = %d dBm", (int)NetworkGetRssi());
    }
  }

  return HX_CMD_OK;
}

// ---------------------------------------------------------------------------
// net connect <ssid> [password]
// ---------------------------------------------------------------------------

static HxCmdStatus CmdNetConnect(const char* args, HxCmdOutput* out) {
  const char* p = CmdSkipWs(args);

  char ssid[64];
  if (!CmdExtractToken(&p, ssid, sizeof(ssid))) {
    CmdOutWriteLine(out, "usage: net connect <ssid> [password]");
    return HX_CMD_USAGE;
  }

  char password[64];
  if (!CmdExtractToken(&p, password, sizeof(password))) {
    password[0] = '\0';
  }

  if (!NetworkConnect(ssid, password)) {
    CmdOutPrintfLine(out, "connect failed: \"%s\"", ssid);
    return HX_CMD_ERROR;
  }

  CmdOutPrintfLine(out, "connecting to \"%s\"", ssid);
  return HX_CMD_OK;
}

// ---------------------------------------------------------------------------
// net disconnect
// ---------------------------------------------------------------------------

static HxCmdStatus CmdNetDisconnect(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: net disconnect");
    return HX_CMD_USAGE;
  }

  NetworkDisconnect();
  CmdOutWriteLine(out, "disconnected");
  return HX_CMD_OK;
}

// ---------------------------------------------------------------------------
// net info
// ---------------------------------------------------------------------------

static HxCmdStatus CmdNetInfo(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: net info");
    return HX_CMD_USAGE;
  }

  HxNetworkState state = NetworkGetState();

  CmdOutWriteLine(out, "network info:");
  CmdOutPrintfLine(out, "  state        = %s", NetworkStateStr(state));
  CmdOutPrintfLine(out, "  wifi         = %s", HX_ENABLE_FEATURE_WIFI        ? "true" : "false");
  CmdOutPrintfLine(out, "  esp_hosted   = %s", HX_ENABLE_FEATURE_ESP_HOSTED  ? "true" : "false");
  CmdOutPrintfLine(out, "  eth          = %s", HX_ENABLE_FEATURE_ETH         ? "true" : "false");

  char ssid[64];
  const HxConfigKeyDef* ssid_key = ConfigFindConfigKey("wifi.ssid");
  if (ssid_key && ConfigValueToString(ssid_key, ssid, sizeof(ssid)) && ssid[0] != '\0') {
    CmdOutPrintfLine(out, "  ssid         = %s", ssid);
  } else {
    CmdOutWriteLine(out,  "  ssid         = (not set)");
  }

#if HX_ENABLE_FEATURE_WIFI
  if (state == HX_NETWORK_STATE_CONNECTED) {
    char ip[32];
    if (NetworkGetIp(ip, sizeof(ip))) {
      CmdOutPrintfLine(out, "  wifi_ip      = %s", ip);
    }
    CmdOutPrintfLine(out, "  rssi         = %d dBm", (int)NetworkGetRssi());
  }
#endif

#if HX_ENABLE_FEATURE_ETH
  CmdOutPrintfLine(out, "  eth_link     = %s", NetworkEthIsUp() ? "up" : "down");
  if (NetworkEthIsUp()) {
    char eth_ip[32];
    if (NetworkEthGetIp(eth_ip, sizeof(eth_ip))) {
      CmdOutPrintfLine(out, "  eth_ip       = %s", eth_ip);
    }
  }
#endif

  return HX_CMD_OK;
}

// ---------------------------------------------------------------------------
// net save  — persist current credentials to NVS
// ---------------------------------------------------------------------------

static HxCmdStatus CmdNetSave(const char* args, HxCmdOutput* out) {
  if (CmdSkipWs(args)[0] != '\0') {
    CmdOutWriteLine(out, "usage: net save");
    return HX_CMD_USAGE;
  }

  if (!ConfigSave()) {
    CmdOutWriteLine(out, "save failed");
    return HX_CMD_ERROR;
  }

  CmdOutWriteLine(out, "network config saved");
  return HX_CMD_OK;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

static const HxCmdDef kNetworkCommands[] = {
  { "net",            CmdNetStatus,     "Show network status" },
  { "net status",     CmdNetStatus,     nullptr },
  { "net connect",    CmdNetConnect,    "Connect to WiFi: net connect <ssid> [password]" },
  { "net disconnect", CmdNetDisconnect, "Disconnect from WiFi" },
  { "net info",       CmdNetInfo,       "Show network configuration and status" },
  { "net save",       CmdNetSave,       "Save network credentials to NVS" }
};

bool CommandRegisterNetwork() {
  for (size_t i = 0; i < (sizeof(kNetworkCommands) / sizeof(kNetworkCommands[0])); i++) {
    if (!CommandRegister(&kNetworkCommands[i])) {
      return false;
    }
  }
  return true;
}

#else  // !HX_ENABLE_MODULE_NETWORK

bool CommandRegisterNetwork() {
  return true;
}

#endif // HX_ENABLE_MODULE_NETWORK
