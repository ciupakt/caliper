/**
 * @file espnow_helper.cpp
 * @brief ESP-NOW Helper Functions Implementation
 * @author System Generated
 * @date 2026-01-04
 * @version 1.0
 */

#include "espnow_helper.h"
#include "error_handler.h"
#include <Arduino.h>

ErrorCode espnow_send_with_retry(
    const uint8_t* mac_addr,
    const void* data,
    size_t len,
    int max_retries,
    int retry_delay_ms)
{
    // Validate parameters
    if (mac_addr == nullptr || data == nullptr || len == 0)
    {
        RECORD_ERROR(ERR_VALIDATION_INVALID_PARAM,
            "Invalid parameters: mac_addr=%p, data=%p, len=%u",
            (void*)mac_addr, (void*)data, (unsigned int)len);
        return ERR_VALIDATION_INVALID_PARAM;
    }

    ErrorCode result = ERR_NONE;
    int attempts = 0;

    while (attempts < max_retries)
    {
        esp_err_t sendResult = esp_now_send(mac_addr, (const uint8_t*)data, len);

        if (sendResult == ESP_OK)
        {
            result = ERR_NONE;
            break;
        }
        else
        {
            attempts++;
            if (attempts < max_retries)
            {
                delay(retry_delay_ms);
            }
        }
    }

    if (attempts >= max_retries)
    {
        RECORD_ERROR(ERR_ESPNOW_SEND_FAILED,
            "ESP-NOW send failed after %d attempts to peer %02X:%02X:%02X:%02X:%02X:%02X",
            attempts,
            mac_addr[0], mac_addr[1], mac_addr[2],
            mac_addr[3], mac_addr[4], mac_addr[5]);
        result = ERR_ESPNOW_SEND_FAILED;
    }

    return result;
}

ErrorCode espnow_add_peer_with_retry(
    esp_now_peer_info_t* peer_info,
    int max_retries,
    int retry_delay_ms)
{
    // Validate parameters
    if (peer_info == nullptr)
    {
        RECORD_ERROR(ERR_VALIDATION_INVALID_PARAM,
            "Invalid parameter: peer_info is null");
        return ERR_VALIDATION_INVALID_PARAM;
    }

    int attempts = 0;
    esp_err_t result;

    while (attempts < max_retries)
    {
        result = esp_now_add_peer(peer_info);

        if (result == ESP_OK)
        {
            return ERR_NONE;
        }

        attempts++;
        if (attempts < max_retries)
        {
            delay(retry_delay_ms);
        }
    }

    RECORD_ERROR(ERR_ESPNOW_PEER_ADD_FAILED,
        "Failed to add peer %02X:%02X:%02X:%02X:%02X:%02X after %d attempts",
        peer_info->peer_addr[0], peer_info->peer_addr[1], peer_info->peer_addr[2],
        peer_info->peer_addr[3], peer_info->peer_addr[4], peer_info->peer_addr[5],
        attempts);

    return ERR_ESPNOW_PEER_ADD_FAILED;
}
