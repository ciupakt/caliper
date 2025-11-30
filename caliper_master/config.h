/**
 * @file config.h
 * @brief Central configuration file for ESP32 Caliper System
 * @author System Generated
 * @date 2025-11-30
 * @version 1.0
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ESP-NOW Configuration
#define ESPNOW_WIFI_CHANNEL 1
#define ESPNOW_RETRY_DELAY_MS 100
#define ESPNOW_MAX_RETRIES 3

// Timing Configuration
#define MEASUREMENT_TIMEOUT_MS 200
#define BATTERY_UPDATE_INTERVAL_MS 1000
#define MOTOR_COMMAND_TIMEOUT_MS 50
#define WEB_UPDATE_INTERVAL_MS 10

// Pin Definitions for Slave
#define CLOCK_PIN 18
#define DATA_PIN 19
#define TRIG_PIN 5
#define BATTERY_VOLTAGE_PIN 34

// Motor Pins
#define MOTOR_IN1_PIN 13
#define MOTOR_IN2_PIN 12

// WiFi Configuration
#define WIFI_SSID "ESP32_Pomiar"
#define WIFI_PASSWORD "12345678"
#define WIFI_AP_IP IPAddress(192, 168, 4, 1)

// Measurement Validation
#define MEASUREMENT_MIN_VALUE -1000.0f
#define MEASUREMENT_MAX_VALUE 1000.0f
#define INVALID_MEASUREMENT_VALUE -999.0f

// ADC Configuration
#define ADC_RESOLUTION 4095
#define ADC_REFERENCE_VOLTAGE_MV 3300

// Web Server Configuration
#define WEB_SERVER_PORT 80
#define HTML_BUFFER_SIZE 2048

// Debug Configuration
#define DEBUG_ENABLED true
#define MAX_LOG_ENTRIES 200

#endif // CONFIG_H