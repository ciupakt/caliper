/**
 * @file config.h
 * @brief Configuration file for ESP32 Caliper Master
 * @author System Generated
 * @date 2025-12-26
 * @version 2.0
 * 
 * This file contains Master-specific configuration.
 * Common settings are inherited from shared/config_base.h
 */

#ifndef CONFIG_MASTER_H
#define CONFIG_MASTER_H

#include <Arduino.h>

// Include shared base configuration
#include <shared_config.h>

// ============================================================================
// Device MAC Address Configuration
// ============================================================================

/**
 * @brief MAC address of the Slave device
 * To find the MAC address, run the slave and check Serial output at startup.
 */
#define SLAVE_MAC_ADDR {0xA0, 0xB7, 0x65, 0x21, 0x77, 0x5C}

// ============================================================================
// WiFi Access Point Configuration
// ============================================================================
#define WIFI_SSID "Orange_WiFi"
#define WIFI_PASSWORD "1670$2026"
#define WIFI_AP_IP IPAddress(192, 168, 4, 1)

// ============================================================================
// Web Server Configuration
// ============================================================================
#define WEB_SERVER_PORT 80
#define HTML_BUFFER_SIZE 2048
#define WEB_UPDATE_INTERVAL_MS 10

// ============================================================================
// Master-specific Settings
// ============================================================================
#define MAX_LOG_ENTRIES 200

#endif // CONFIG_MASTER_H
