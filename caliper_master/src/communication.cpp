/**
 * @file communication.cpp
 * @brief ESP-NOW Communication Module Implementation
 * @author System Generated
 * @date 2025-11-30
 * @version 1.0
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
    lastError = ERR_INVALID_DATA;
    return lastError;
  }

  // Copy slave address
  memcpy(slaveAddress, slaveAddr, 6);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    lastError = ERR_ESPNOW_SEND;
    return lastError;
  }

  // Configure peer info
  memcpy(peerInfo.peer_addr, slaveAddress, 6);
  peerInfo.channel = ESPNOW_WIFI_CHANNEL;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    lastError = ERR_ESPNOW_SEND;
    return lastError;
  }

  initialized = true;
  lastError = ERR_NONE;
  return lastError;
}

ErrorCode CommunicationManager::sendCommand(CommandType command, int retryCount)
{
  if (!initialized)
  {
    lastError = ERR_ESPNOW_SEND;
    return lastError;
  }

  ErrorCode result = ERR_NONE;
  int attempts = 0;

  while (attempts < retryCount)
  {
    esp_err_t sendResult = esp_now_send(slaveAddress, (uint8_t *)&command, sizeof(command));

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
    result = ERR_ESPNOW_SEND;
  }

  lastError = result;
  return result;
}

ErrorCode CommunicationManager::sendMessage(const Message &message, int retryCount)
{
  if (!initialized)
  {
    lastError = ERR_ESPNOW_SEND;
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
    result = ERR_ESPNOW_SEND;
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