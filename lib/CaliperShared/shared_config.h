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
#define CALIPER_CLOCK_PIN 11
#define CALIPER_DATA_PIN 12
#define CALIPER_TRIG_PIN 13

// ============================================================================
// Pin Definitions - Motor (STSPIN250)
// ============================================================================
#define MOTOR_PWM_PIN 6    // PWM input - speed control
#define MOTOR_PH_PIN 15    // Phase input - direction control
#define MOTOR_REF_PIN 7    // REF input - current limit via PWM + RC filter
#define MOTOR_EN_PIN 16    // Enable pin - HIGH = enabled
#define MOTOR_FAULT_PIN 17 // Fault input - LOW = fault detected

// ============================================================================
// Pin Definitions - Battery
// ============================================================================
#define BATTERY_VOLTAGE_PIN 10

// ============================================================================
// Calibration Configuration
// ============================================================================
#define CALIBRATION_OFFSET_MIN -14.999f
#define CALIBRATION_OFFSET_MAX 14.999f

// ============================================================================
// Session Configuration
// ============================================================================
#define SESSION_NAME_MIN_LENGTH 1
#define SESSION_NAME_MAX_LENGTH 31

// ============================================================================
// Timing Configuration (Additional)
// ============================================================================
#define MEASUREMENT_TIMEOUT_MARGIN_MS 1000
#define POLL_DELAY_MS 1
#define TIMER_DELAY_MS 1
#define WIFI_INIT_DELAY_MS 100
#define WIFI_MAX_ATTEMPTS 10
#define WIFI_RETRY_DELAY_MS 100
#define PEER_MAX_ATTEMPTS 10
#define PEER_RETRY_DELAY_MS 100

// ============================================================================
// Caliper Configuration
// ============================================================================
#define CALIPER_BIT_BUFFER_SIZE 52
#define CALIPER_BIT_SHIFT 8
#define CALIPER_NIBBLE_COUNT 13
#define BITS_PER_NIBBLE 4
#define CALIPER_DECIMAL_DIGITS 5
#define CALIPER_VALUE_DIVISOR 1000.0f
#define INCH_TO_MM_FACTOR 25.4f

// ============================================================================
// Motor Configuration
// ============================================================================
#define PWM_MAX_VALUE 255
#define MOTOR_SPEED_CHANGE_THRESHOLD 10

// ============================================================================
// Buffer Sizes
// ============================================================================
#define LAST_MEASUREMENT_BUFFER_SIZE 64
#define LAST_BATTERY_VOLTAGE_BUFFER_SIZE 32
#define JSON_RESPONSE_BUFFER_SIZE 512

// ============================================================================
// Debug Configuration
// ============================================================================
#define DEBUG_ENABLED true

#endif // SHARED_CONFIG_BASE_H
