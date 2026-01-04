/**
 * @file espnow_helper.h
 * @brief ESP-NOW Helper Functions for Master and Slave
 * @author System Generated
 * @date 2026-01-04
 * @version 1.0
 *
 * This module provides unified ESP-NOW communication functions with retry logic
 * for both Master and Slave devices. It eliminates code duplication and ensures
 * consistent error handling across the system.
 */

#ifndef ESPNOW_HELPER_H
#define ESPNOW_HELPER_H

#include <stdint.h>
#include <esp_now.h>
#include "shared_config.h"
#include "error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send data via ESP-NOW with automatic retry mechanism
 *
 * This function attempts to send data via ESP-NOW and automatically retries
 * on failure. It provides consistent retry behavior across Master and Slave.
 *
 * @param mac_addr MAC address of the peer device
 * @param data Pointer to data buffer to send
 * @param len Length of data in bytes
 * @param max_retries Maximum number of retry attempts (default: ESPNOW_MAX_RETRIES)
 * @param retry_delay_ms Delay between retry attempts in ms (default: ESPNOW_RETRY_DELAY_MS)
 *
 * @return ERR_NONE if successful, error code otherwise
 *
 * Possible errors:
 * - ERR_VALIDATION_INVALID_PARAM: Invalid parameters (null pointer or zero length)
 * - ERR_ESPNOW_SEND_FAILED: Send operation failed after all retry attempts
 *
 * Example usage:
 * @code
 * ErrorCode result = espnow_send_with_retry(
 *     masterAddress,
 *     &msgSlave,
 *     sizeof(msgSlave)
 * );
 * if (result != ERR_NONE) {
 *     // Handle error
 * }
 * @endcode
 */
ErrorCode espnow_send_with_retry(
    const uint8_t* mac_addr,
    const void* data,
    size_t len,
    int max_retries = ESPNOW_MAX_RETRIES,
    int retry_delay_ms = ESPNOW_RETRY_DELAY_MS
);

/**
 * @brief Add ESP-NOW peer with retry mechanism
 *
 * This function attempts to add a peer to ESP-NOW and automatically retries
 * on failure. Useful for both Master and Slave initialization.
 *
 * @param peer_info Pointer to peer info structure
 * @param max_retries Maximum number of retry attempts (default: PEER_MAX_ATTEMPTS)
 * @param retry_delay_ms Delay between retry attempts in ms (default: PEER_RETRY_DELAY_MS)
 *
 * @return ERR_NONE if successful, error code otherwise
 *
 * Possible errors:
 * - ERR_VALIDATION_INVALID_PARAM: Invalid parameters (null pointer)
 * - ERR_ESPNOW_PEER_ADD_FAILED: Peer addition failed after all retry attempts
 *
 * Example usage:
 * @code
 * esp_now_peer_info_t peerInfo;
 * memcpy(peerInfo.peer_addr, masterAddress, 6);
 * peerInfo.channel = ESPNOW_WIFI_CHANNEL;
 * peerInfo.encrypt = false;
 *
 * ErrorCode result = espnow_add_peer_with_retry(&peerInfo);
 * if (result != ERR_NONE) {
 *     // Handle error
 * }
 * @endcode
 */
ErrorCode espnow_add_peer_with_retry(
    esp_now_peer_info_t* peer_info,
    int max_retries = PEER_MAX_ATTEMPTS,
    int retry_delay_ms = PEER_RETRY_DELAY_MS
);

#ifdef __cplusplus
}
#endif

#endif // ESPNOW_HELPER_H
