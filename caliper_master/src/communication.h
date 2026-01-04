/**
 * @file communication.h
 * @brief ESP-NOW Communication Module Header
 * @author System Generated
 * @date 2025-11-30
 * @version 2.0
 *
 * @version 2.0 - Integrated comprehensive error code system
 */

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "config.h"
#include <shared_common.h>
#include <error_handler.h>
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
   *
   * Possible errors:
   * - ERR_ESPNOW_INIT_FAILED: ESP-NOW initialization failed
   * - ERR_ESPNOW_PEER_ADD_FAILED: Peer addition failed
   */
  ErrorCode initialize(const uint8_t *slaveAddr);

  /**
   * @brief Send message to slave device
   * @param message MessageMaster payload to send
   * @param retryCount Number of retries (default: ESPNOW_MAX_RETRIES)
   * @return ERR_NONE if successful, error code otherwise
   *
   * Possible errors:
   * - ERR_ESPNOW_SEND_FAILED: Send operation failed after retries
   */
  ErrorCode sendMessage(const MessageMaster &message, int retryCount = ESPNOW_MAX_RETRIES);

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