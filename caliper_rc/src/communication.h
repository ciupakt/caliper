/**
 * @file communication.h
 * @brief ESP-NOW Communication Module Header for RC device
 * @date 2026-05-03
 * @version 1.0
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
  uint8_t masterAddress[6];
  esp_now_peer_info_t peerInfo;
  bool initialized;
  ErrorCode lastError;

public:
  CommunicationManager();

  ErrorCode initialize(const uint8_t *masterAddr);

  ErrorCode sendMessage(const MessageRC &message, int retryCount = ESPNOW_MAX_RETRIES);

  bool isInitialized() const { return initialized; }

  ErrorCode getLastError() const { return lastError; }

  void setReceiveCallback(esp_now_recv_cb_t callback);

  void setSendCallback(esp_now_send_cb_t callback);
};

#endif // COMMUNICATION_H
