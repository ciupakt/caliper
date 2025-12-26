/**
 * @file config.h
 * @brief Configuration file for ESP32 Caliper Slave
 * @author System Generated
 * @date 2025-12-26
 * @version 2.0
 * 
 * This file contains Slave-specific configuration.
 * Common settings are inherited from shared/config_base.h
 */

#ifndef CONFIG_SLAVE_H
#define CONFIG_SLAVE_H

#include <Arduino.h>

// Include shared base configuration
#include <shared_config.h>

// ============================================================================
// Device MAC Address Configuration
// ============================================================================

/**
 * @brief MAC address of the Master device
 * To find the MAC address, run the master and check Serial output at startup.
 */
#define MASTER_MAC_ADDR {0xA0, 0xB7, 0x65, 0x20, 0xC0, 0x8C}

// ============================================================================
// Slave-specific Pin Aliases (for backward compatibility)
// ============================================================================
// These use the definitions from config_base.h
#define CLOCK_PIN CALIPER_CLOCK_PIN
#define DATA_PIN CALIPER_DATA_PIN
#define TRIG_PIN CALIPER_TRIG_PIN

// ============================================================================
// Slave-specific Settings
// ============================================================================
#define ADC_SAMPLES 8

#endif // CONFIG_SLAVE_H
