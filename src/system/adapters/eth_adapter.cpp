/*
  HexaOS - eth_adapter.cpp

  Copyright (C) 2026 Martin Macak
  SPDX-License-Identifier: GPL-3.0-only

  Description
  Ethernet hardware adapter implementation.

  Initialisation order (EthAdapterInit):
    1. Resolve ETH0 RMII pins from the pinmap:
       MDC, MDIO, POWER, TX_EN, TXD0, TXD1, RXD0, RXD1, CRS_DV, REF_CLK.
       MDC, MDIO, TX_EN, TXD0/1, RXD0/1 and CRS_DV are mandatory.
       POWER and REF_CLK are used when >= 0.
    2. Build eth_esp32_emac_config_t from resolved pins, HX_ETH_CLK_MODE
       and HX_ETH_PHY_ADDR.
    3. Build eth_phy_config_t; select PHY constructor via HX_ETH_PHY_TYPE.
    4. Install the ESP-IDF Ethernet driver (esp_eth_driver_install).
    5. Create ESP_NETIF_DEFAULT_ETH netif and attach the netif glue.
    6. Register ETH_EVENT / IP_EVENT handlers.
    7. Call esp_eth_start() — link events follow when cable is connected.

  Event flow:
    IDF event loop → EthEventHandler → updates g_link_up / g_has_ip,
    fires g_event_cb. Hx.net_* flags are managed by network_handler.

  The registered event callback is called from the IDF event-loop task.
  Consumers must not block inside it.
*/

#include "eth_adapter.h"

#include "headers/hx_build.h"

#if HX_ENABLE_FEATURE_ETH

#include "esp_eth.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "headers/hx_pinfunc.h"
#include "system/core/log.h"
#include "system/core/pinmap.h"

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static constexpr const char* HX_ETH_TAG = "ETH";

static bool              g_link_up       = false;
static bool              g_has_ip        = false;
static EthAdapterEventCb g_event_cb      = nullptr;
static void*             g_event_cb_user = nullptr;
static esp_eth_handle_t  g_eth_handle    = NULL;
static esp_netif_t*      g_eth_netif     = NULL;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void FireEvent(EthAdapterEvent event) {
  if (g_event_cb) {
    g_event_cb(event, g_event_cb_user);
  }
}

static void EthEventHandler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data) {
  (void)arg;

  if (event_base == ETH_EVENT) {
    if (event_id == ETHERNET_EVENT_CONNECTED) {
      g_link_up = true;
      HX_LOGI(HX_ETH_TAG, "link up");
      FireEvent(ETH_ADAPTER_EVENT_LINK_UP);

    } else if (event_id == ETHERNET_EVENT_DISCONNECTED) {
      g_link_up = false;
      g_has_ip  = false;
      HX_LOGI(HX_ETH_TAG, "link down");
      FireEvent(ETH_ADAPTER_EVENT_LINK_DOWN);
    }

  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_ETH_GOT_IP) {
      ip_event_got_ip_t* ev = static_cast<ip_event_got_ip_t*>(event_data);
      g_has_ip = true;
      HX_LOGI(HX_ETH_TAG, "IP acquired: " IPSTR, IP2STR(&ev->ip_info.ip));
      FireEvent(ETH_ADAPTER_EVENT_IP_ACQUIRED);

    } else if (event_id == IP_EVENT_ETH_LOST_IP) {
      g_has_ip = false;
      HX_LOGW(HX_ETH_TAG, "IP lost");
      FireEvent(ETH_ADAPTER_EVENT_IP_LOST);
    }
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool EthAdapterInit() {
  int mdc     = PinmapGetGpioForFunction(HX_PIN_ETH0_MDC);
  int mdio    = PinmapGetGpioForFunction(HX_PIN_ETH0_MDIO);
  int power   = PinmapGetGpioForFunction(HX_PIN_ETH0_POWER);
  int tx_en   = PinmapGetGpioForFunction(HX_PIN_ETH0_RMII_TX_EN);
  int txd0    = PinmapGetGpioForFunction(HX_PIN_ETH0_RMII_TXD0);
  int txd1    = PinmapGetGpioForFunction(HX_PIN_ETH0_RMII_TXD1);
  int rxd0    = PinmapGetGpioForFunction(HX_PIN_ETH0_RMII_RXD0);
  int rxd1    = PinmapGetGpioForFunction(HX_PIN_ETH0_RMII_RXD1);
  int crs_dv  = PinmapGetGpioForFunction(HX_PIN_ETH0_RMII_CRS_DV);
  int ref_clk = PinmapGetGpioForFunction(HX_PIN_ETH0_RMII_REF_CLK);

  if (mdc < 0 || mdio < 0 || tx_en < 0 || txd0 < 0 || txd1 < 0 ||
      rxd0 < 0 || rxd1 < 0 || crs_dv < 0) {
    HX_LOGE(HX_ETH_TAG,
            "mandatory RMII pins not mapped "
            "(mdc=%d mdio=%d tx_en=%d txd0=%d txd1=%d rxd0=%d rxd1=%d crs_dv=%d)",
            mdc, mdio, tx_en, txd0, txd1, rxd0, rxd1, crs_dv);
    return false;
  }

  HX_LOGI(HX_ETH_TAG,
           "RMII: mdc=%d mdio=%d power=%d tx_en=%d txd0=%d txd1=%d "
           "rxd0=%d rxd1=%d crs_dv=%d ref_clk=%d",
           mdc, mdio, power, tx_en, txd0, txd1, rxd0, rxd1, crs_dv, ref_clk);

  // MAC — use member assignment to avoid C++ designated-initializer order
  // issues with ETH_ESP32_EMAC_DEFAULT_CONFIG() (its macro field order differs
  // from the struct declaration order on ESP32-P4).
  eth_mac_config_t mac_config         = ETH_MAC_DEFAULT_CONFIG();
  eth_esp32_emac_config_t emac_config = {};

  emac_config.smi_gpio.mdc_num  = mdc;
  emac_config.smi_gpio.mdio_num = mdio;
  emac_config.interface         = EMAC_DATA_INTERFACE_RMII;

  emac_config.clock_config.rmii.clock_mode = HX_ETH_CLK_MODE;
  emac_config.clock_config.rmii.clock_gpio = static_cast<emac_rmii_clock_gpio_t>(ref_clk);

  emac_config.dma_burst_len  = ETH_DMA_BURST_LEN_32;
  emac_config.intr_priority  = 0;
  emac_config.mdc_freq_hz    = 0;

  emac_config.emac_dataif_gpio.rmii.tx_en_num  = tx_en;
  emac_config.emac_dataif_gpio.rmii.txd0_num   = txd0;
  emac_config.emac_dataif_gpio.rmii.txd1_num   = txd1;
  emac_config.emac_dataif_gpio.rmii.crs_dv_num = crs_dv;
  emac_config.emac_dataif_gpio.rmii.rxd0_num   = rxd0;
  emac_config.emac_dataif_gpio.rmii.rxd1_num   = rxd1;

  esp_eth_mac_t* mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
  if (!mac) {
    HX_LOGE(HX_ETH_TAG, "MAC init failed");
    return false;
  }

  // PHY
  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
  phy_config.phy_addr            = HX_ETH_PHY_ADDR;
  phy_config.reset_gpio_num      = power;  // -1 when not mapped — no hard reset
  // Do not block boot waiting for auto-negotiation. Link state arrives via
  // ETHERNET_EVENT_CONNECTED event; 100 ms is enough for a cable-present check.
  phy_config.autonego_timeout_ms = 500; //100 ms causing boot loop when eth init. Avoid. Min 500ms

#if HX_ETH_PHY_TYPE == HX_ETH_PHY_TLK110
  // TLK110 has no dedicated IDF driver on ESP32-P4; it is IEEE 802.3 compliant
  // so the generic driver works via standard MII/RMII register access.
  esp_eth_phy_t* phy = esp_eth_phy_new_generic(&phy_config);
#elif HX_ETH_PHY_TYPE == HX_ETH_PHY_GENERIC
  esp_eth_phy_t* phy = esp_eth_phy_new_generic(&phy_config);
#elif HX_ETH_PHY_TYPE == HX_ETH_PHY_LAN8720
  esp_eth_phy_t* phy = esp_eth_phy_new_lan87xx(&phy_config);
#elif HX_ETH_PHY_TYPE == HX_ETH_PHY_KSZ8081
  esp_eth_phy_t* phy = esp_eth_phy_new_ksz80xx(&phy_config);
#elif HX_ETH_PHY_TYPE == HX_ETH_PHY_DP83848
  esp_eth_phy_t* phy = esp_eth_phy_new_dp83848(&phy_config);
#else
  #error "eth_adapter: unsupported HX_ETH_PHY_TYPE — add constructor here"
#endif

  if (!phy) {
    HX_LOGE(HX_ETH_TAG, "PHY init failed");
    mac->del(mac);
    return false;
  }

  // Driver install
  esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
  esp_err_t err = esp_eth_driver_install(&eth_config, &g_eth_handle);
  if (err != ESP_OK) {
    HX_LOGE(HX_ETH_TAG, "driver install failed: %s", esp_err_to_name(err));
    mac->del(mac);
    phy->del(phy);
    return false;
  }

  // Netif
  esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
  g_eth_netif = esp_netif_new(&netif_config);
  if (!g_eth_netif) {
    HX_LOGE(HX_ETH_TAG, "netif create failed");
    esp_eth_driver_uninstall(g_eth_handle);
    g_eth_handle = NULL;
    return false;
  }

  esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(g_eth_handle);
  err = esp_netif_attach(g_eth_netif, glue);
  if (err != ESP_OK) {
    HX_LOGE(HX_ETH_TAG, "netif attach failed: %s", esp_err_to_name(err));
    esp_eth_driver_uninstall(g_eth_handle);
    g_eth_handle = NULL;
    return false;
  }

  // Event handlers (event loop must already be running — wifi_adapter creates it)
  esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &EthEventHandler, nullptr);
  esp_event_handler_register(IP_EVENT,  ESP_EVENT_ANY_ID, &EthEventHandler, nullptr);

  err = esp_eth_start(g_eth_handle);
  if (err != ESP_OK) {
    HX_LOGE(HX_ETH_TAG, "start failed: %s", esp_err_to_name(err));
    return false;
  }

  HX_LOGI(HX_ETH_TAG, "init OK (phy_type=%d phy_addr=%d clk_mode=%d)",
           HX_ETH_PHY_TYPE, HX_ETH_PHY_ADDR, (int)HX_ETH_CLK_MODE);
  return true;
}

bool EthAdapterIsLinkUp() {
  return g_link_up;
}

bool EthAdapterHasIp() {
  return g_has_ip;
}

bool EthAdapterGetIp(char* out, size_t out_size) {
  if (!out || out_size == 0) {
    return false;
  }
  if (!g_has_ip || !g_eth_netif) {
    out[0] = '\0';
    return false;
  }
  esp_netif_ip_info_t ip_info;
  if (esp_netif_get_ip_info(g_eth_netif, &ip_info) != ESP_OK) {
    out[0] = '\0';
    return false;
  }
  snprintf(out, out_size, IPSTR, IP2STR(&ip_info.ip));
  return true;
}

esp_netif_t* EthAdapterGetNetif() {
  return g_eth_netif;
}

void EthAdapterSetEventCallback(EthAdapterEventCb cb, void* user) {
  g_event_cb      = cb;
  g_event_cb_user = user;
}

#endif // HX_ENABLE_FEATURE_ETH
