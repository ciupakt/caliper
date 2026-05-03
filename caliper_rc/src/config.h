/**
 * @file config.h
 * @brief Configuration file for ESP32 Caliper RC
 * @date 2026-05-03
 * @version 1.0
 */

#ifndef CONFIG_RC_H
#define CONFIG_RC_H

#include <Arduino.h>

#include <shared_config.h>

// ============================================================================
// Device MAC Address Configuration
// ============================================================================

#define MASTER_MAC_ADDR {0x10, 0xB4, 0x1D, 0xD6, 0x40, 0xBB}

// ============================================================================
// Button Pin Configuration (ESP32-C3 Super Mini)
// ============================================================================

#define BUTTON_TRIG_PIN 8
#define BUTTON_DROP_PIN 9
#define DEBOUNCE_DELAY_MS 50

// ============================================================================
// LED Indicator
// ============================================================================

#define LED_PIN LED_GREEN

#endif // CONFIG_RC_H
