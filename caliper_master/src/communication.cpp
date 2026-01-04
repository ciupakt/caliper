/**
 * @file communication.cpp
 * @brief ESP-NOW Communication Module Implementation
 * @author System Generated
 * @date 2025-11-30
 * @version 2.0
 *
 * @version 2.0 - Integrated comprehensive error code system
 */

#include "communication.h"

CommunicationManager::CommunicationManager() : initialized(false), lastError(ERR_NONE)
{
  // Initialize with default values
  memset(slaveAddress, 0, 6);
  memset(&peerInfo, 0, sizeof(peerInfo));
}

ErrorCode CommunicationManager::initialize(const uint8_t *slaveAddr)
{
  if (!slaveAddr)
  {
    RECORD_ERROR(ERR_VALIDATION_INVALID_PARAM, "Null slave address provided");
    lastError = ERR_VALIDATION_INVALID_PARAM;
    return lastError;
  }

  // Copy slave address
  memcpy(slaveAddress, slaveAddr, 6);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    RECORD_ERROR(ERR_ESPNOW_INIT_FAILED, "ESP-NOW initialization failed");
    lastError = ERR_ESPNOW_INIT_FAILED;
    return lastError;
  }

  // Configure peer info
  memcpy(peerInfo.peer_addr, slaveAddress, 6);
  peerInfo.channel = ESPNOW_WIFI_CHANNEL;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    RECORD_ERROR(ERR_ESPNOW_PEER_ADD_FAILED, "Failed to add peer: %02X:%02X:%02X:%02X:%02X:%02X",
      slaveAddress[0], slaveAddress[1], slaveAddress[2], slaveAddress[3], slaveAddress[4], slaveAddress[5]);
    lastError = ERR_ESPNOW_PEER_ADD_FAILED;
    return lastError;
  }

  initialized = true;
  lastError = ERR_NONE;
  return lastError;
}

ErrorCode CommunicationManager::sendMessage(const MessageMaster &message, int retryCount)
{
  if (!initialized)
  {
    RECORD_ERROR(ERR_ESPNOW_SEND_FAILED, "Communication manager not initialized");
    lastError = ERR_ESPNOW_SEND_FAILED;
    return lastError;
  }

  ErrorCode result = ERR_NONE;
  int attempts = 0;

  while (attempts < retryCount)
  {
    esp_err_t sendResult = esp_now_send(slaveAddress, (uint8_t *)&message, sizeof(message));

    if (sendResult == ESP_OK)
    {
      result = ERR_NONE;
      break;
    }
    else
    {
      attempts++;
      if (attempts < retryCount)
      {
        delay(ESPNOW_RETRY_DELAY_MS);
      }
    }
  }

  if (attempts >= retryCount)
  {
    RECORD_ERROR(ERR_ESPNOW_SEND_FAILED, "Send failed after %d attempts to peer %02X:%02X:%02X:%02X:%02X:%02X",
      attempts, slaveAddress[0], slaveAddress[1], slaveAddress[2], slaveAddress[3], slaveAddress[4], slaveAddress[5]);
    result = ERR_ESPNOW_SEND_FAILED;
  }

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