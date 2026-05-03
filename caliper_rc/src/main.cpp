#include <esp_now.h>
#include <WiFi.h>
#include "config.h"
#include <shared_common.h>
#include <error_handler.h>
#include <MacroDebugger.h>
#include <arduino-timer.h>
#include "communication.h"

// Slave device MAC address (defined in config.h)
uint8_t masterAddress[] = MASTER_MAC_ADDR;

CommunicationManager commManager;

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
  (void)recv_info;

}

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
  (void)info;
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    DEBUG_I("Status wysyłki: Sukces");
  }
  else
  {
    RECORD_ERROR(ERR_ESPNOW_SEND_FAILED, "ESP-NOW send callback reported failure");
  }
}

void setup()
{
  DEBUG_BEGIN();
  DEBUG_I("=== ESP32 RC - Suwmiarka + ESP-NOW ===");

  
  // Initialize error handler
  ERROR_HANDLER.initialize();
  

  // Setup WiFi
  WiFi.mode(WIFI_AP_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  DEBUG_I("MAC Address RC: %s", WiFi.macAddress().c_str());
  DEBUG_I("");

  // Initialize communication manager
  ErrorCode commResult = commManager.initialize(masterAddress);
  if (commResult != ERR_NONE)
  {
    LOG_ERROR(commResult, "Failed to initialize ESP-NOW communication");
    return;
  }

}

void loop()
{

}
