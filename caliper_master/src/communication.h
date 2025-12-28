/**
 * @file communication.h
 * @brief ESP-NOW Communication Module Header
 * @author System Generated
 * @date 2025-11-30
 * @version 1.0
 */

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "config.h"
#include <shared_common.h>
#include <esp_now.h>
#include <WiFi.h>

class CommunicationManager
{
private:
  uint8_t slaveAddress[6];
  esp_now_peer_info_t peerInfo;
  bool initialized;
  ErrorCode lastError;

public:
  CommunicationManager();

  /**
   * @brief Initialize ESP-NOW communication
   * @param slaveAddr MAC address of slave device
   * @return ERR_NONE if successful, error code otherwise
   */
  ErrorCode initialize(const uint8_t *slaveAddr);

  /**
   * @brief Send message to slave device
   * @param message Message to send
   * @param retryCount Number of retries (default: ESPNOW_MAX_RETRIES)
   * @return ERR_NONE if successful, error code otherwise
   */
  ErrorCode sendMessage(const Message &message, int retryCount = ESPNOW_MAX_RETRIES);

  /**
   * @brief Check if communication is initialized
   * @return true if initialized, false otherwise
   */
  bool isInitialized() const { return initialized; }

  /**
   * @brief Get last error
   * @return Last error code
   */
  ErrorCode getLastError() const { return lastError; }

  /**
   * @brief Set receive callback
   * @param callback Function to call when data is received
   */
  void setReceiveCallback(esp_now_recv_cb_t callback);

  /**
   * @brief Set send callback
   * @param callback Function to call when data is sent
   */
  void setSendCallback(esp_now_send_cb_t callback);
};

#endif // COMMUNICATION_H