/**
 * @file common.h
 * @brief Common definitions and structures for ESP32 Caliper Slave System
 * @author System Generated
 * @date 2025-11-30
 * @version 1.0
 */

#ifndef COMMON_SLAVE_H
#define COMMON_SLAVE_H

#include <stdint.h>

// Command types for ESP-NOW communication
enum CommandType : char {
  CMD_MEASURE = 'M',
  CMD_FORWARD = 'F',
  CMD_REVERSE = 'R',
  CMD_STOP = 'S',
  CMD_UPDATE = 'U'
};

// Motor state enumeration
enum MotorState : uint8_t {
  MOTOR_STOP = 0,     /**< Motor stopped (coast) */
  MOTOR_FORWARD = 1,  /**< Motor rotating forward */
  MOTOR_REVERSE = 2,  /**< Motor rotating reverse */
  MOTOR_BRAKE = 3     /**< Motor braking */
};

// Error codes
enum ErrorCode : uint8_t {
  ERR_NONE = 0,
  ERR_ESPNOW_SEND,
  ERR_MEASUREMENT_TIMEOUT,
  ERR_INVALID_DATA,
  ERR_ADC_READ,
  ERR_INVALID_COMMAND
};

// Communication message structure
struct Message {
  float measurement;        /**< Measurement value in mm */
  bool valid;              /**< Whether measurement is valid */
  uint32_t timestamp;       /**< Timestamp from system start */
  CommandType command;      /**< Command type */
  uint16_t batteryVoltage;  /**< Battery voltage in millivolts */
};

#endif // COMMON_SLAVE_H