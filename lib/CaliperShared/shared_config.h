/**
 * @file config_base.h
 * @brief Shared configuration parameters for ESP32 Caliper System
 * @author System Generated
 * @date 2025-12-26
 * @version 2.0
 * 
 * This file contains common configuration parameters shared between
 * Master and Slave devices. Device-specific settings should be defined
 * in their respective config.h files.
 */

#ifndef SHARED_CONFIG_BASE_H
#define SHARED_CONFIG_BASE_H

// ============================================================================
// ESP-NOW Configuration
// ============================================================================
#define ESPNOW_WIFI_CHANNEL 1
#define ESPNOW_RETRY_DELAY_MS 100
#define ESPNOW_MAX_RETRIES 3

// ============================================================================
// Timing Configuration
// ============================================================================
#define MEASUREMENT_TIMEOUT_MS 200
#define BATTERY_UPDATE_INTERVAL_MS 1000
#define MOTOR_COMMAND_TIMEOUT_MS 50

// ============================================================================
// Measurement Validation
// ============================================================================
#define MEASUREMENT_MIN_VALUE -1000.0f
#define MEASUREMENT_MAX_VALUE 1000.0f
#define INVALID_MEASUREMENT_VALUE -999.0f

// ============================================================================
// ADC Configuration
// ============================================================================
#define ADC_RESOLUTION 4095
#define ADC_REFERENCE_VOLTAGE_MV 3300

// ============================================================================
// Pin Definitions - Caliper
// ============================================================================
#define CALIPER_CLOCK_PIN 18
#define CALIPER_DATA_PIN 19
#define CALIPER_TRIG_PIN 5

// ============================================================================
// Pin Definitions - Motor (MP6550GG-Z)
// ============================================================================
#define MOTOR_IN1_PIN 13
#define MOTOR_IN2_PIN 12

// ============================================================================
// Pin Definitions - Battery
// ============================================================================
#define BATTERY_VOLTAGE_PIN 34

// ============================================================================
// Debug Configuration
// ============================================================================
#define DEBUG_ENABLED true

#endif // SHARED_CONFIG_BASE_H
