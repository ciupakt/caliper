/**
 * @file communication.cpp
 * @brief ESP-NOW Communication Module Implementation for RC device
 * @date 2026-05-03
 * @version 1.0
 */

#include "communication.h"
#include <espnow_helper.h>

CommunicationManager::CommunicationManager() : initialized(false), lastError(ERR_NONE)
{
  memset(masterAddress, 0, 6);
  memset(&peerInfo, 0, sizeof(peerInfo));
}

ErrorCode CommunicationManager::initialize(const uint8_t *masterAddr)
{
  if (!masterAddr)
  {
    RECORD_ERROR(ERR_VALIDATION_INVALID_PARAM, "Null master address provided");
    lastError = ERR_VALIDATION_INVALID_PARAM;
    return lastError;
  }

  memcpy(masterAddress, masterAddr, 6);

  if (esp_now_init() != ESP_OK)
  {
    RECORD_ERROR(ERR_ESPNOW_INIT_FAILED, "ESP-NOW initialization failed");
    lastError = ERR_ESPNOW_INIT_FAILED;
    return lastError;
  }

  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = ESPNOW_WIFI_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    RECORD_ERROR(ERR_ESPNOW_PEER_ADD_FAILED, "Failed to add peer: %02X:%02X:%02X:%02X:%02X:%02X",
      masterAddress[0], masterAddress[1], masterAddress[2], masterAddress[3], masterAddress[4], masterAddress[5]);
    lastError = ERR_ESPNOW_PEER_ADD_FAILED;
    return lastError;
  }

  initialized = true;
  lastError = ERR_NONE;
  return lastError;
}

ErrorCode CommunicationManager::sendMessage(const MessageRC &message, int retryCount)
{
  if (!initialized)
  {
    RECORD_ERROR(ERR_ESPNOW_SEND_FAILED, "Communication manager not initialized");
    lastError = ERR_ESPNOW_SEND_FAILED;
    return lastError;
  }

  ErrorCode result = espnow_send_with_retry(
      masterAddress,
      &message,
      sizeof(message),
      retryCount,
      ESPNOW_RETRY_DELAY_MS
  );

  lastError = result;
  return result;
}

void CommunicationManager::setReceiveCallback(esp_now_recv_cb_t callback)
{
  if (initialized && callback)
  {
    esp_now_register_recv_cb(callback);
  }
}

void CommunicationManager::setSendCallback(esp_now_send_cb_t callback)
{
  if (initialized && callback)
  {
    esp_now_register_send_cb(callback);
  }
}
