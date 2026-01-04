/**
 * @file common.h
 * @brief Shared definitions and structures for ESP32 Caliper System
 * @author System Generated
 * @date 2025-12-26
 * @version 3.0
 *
 * This is the unified common header file for both Master and Slave devices.
 * Use build flags to enable device-specific features:
 * - CALIPER_MASTER: Enables Master-specific structures
 * - CALIPER_SLAVE: Enables Slave-specific structures
 *
 * @version 3.0 - Added comprehensive error code system integration
 */

#ifndef SHARED_COMMON_H
#define SHARED_COMMON_H

#include <stdint.h>
#include "error_codes.h"

/**
 * @brief Command types for ESP-NOW communication
 */
enum CommandType : char
{
  CMD_MEASURE = 'M',   /**< Request measurement from slave */
  CMD_UPDATE = 'U',    /**< Request update status from slave */
  CMD_MOTORTEST = 'T', /**< Generic motor control command (uses motorState/motorSpeed/motorTorque) */
};

/**
 * @brief Motor state enumeration
 */
enum MotorState : uint8_t
{
  MOTOR_STOP = 0,    /**< Motor stopped (coast) */
  MOTOR_FORWARD = 1, /**< Motor rotating forward */
  MOTOR_REVERSE = 2, /**< Motor rotating reverse */
  MOTOR_BRAKE = 3    /**< Motor braking */
};

/**
 * @brief Legacy error codes for backward compatibility
 *
 * @deprecated Use ErrorCode from error_codes.h instead
 * This enum is kept for backward compatibility only.
 * New code should use comprehensive error code system defined in error_codes.h.
 *
 * Mapping to new error codes:
 * - ERR_LEGACY_NONE -> ERR_NONE (0x0000)
 * - ERR_LEGACY_ESPNOW_SEND -> ERR_ESPNOW_SEND_FAILED (0x0102)
 * - ERR_LEGACY_MEASUREMENT_TIMEOUT -> ERR_CALIPER_TIMEOUT (0x0201)
 * - ERR_LEGACY_INVALID_DATA -> ERR_CALIPER_INVALID_DATA (0x0202)
 * - ERR_LEGACY_ADC_READ -> ERR_ADC_READ_FAILED (0x0403)
 * - ERR_LEGACY_INVALID_COMMAND -> ERR_VALIDATION_INVALID_COMMAND (0x0705)
 */
enum ErrorCodeLegacy : uint8_t
{
  ERR_LEGACY_NONE = 0,             /**< No error */
  ERR_LEGACY_ESPNOW_SEND,          /**< ESP-NOW send failed */
  ERR_LEGACY_MEASUREMENT_TIMEOUT,  /**< Measurement timeout */
  ERR_LEGACY_INVALID_DATA,         /**< Invalid data received */
  ERR_LEGACY_ADC_READ,             /**< ADC read error */
  ERR_LEGACY_INVALID_COMMAND       /**< Invalid command received */
};

// Note: ErrorCode is now defined in error_codes.h as uint16_t
// The comprehensive error code system provides:
// - 8 error categories
// - 10 source modules
// - Detailed error descriptions and recovery actions
// - Helper functions for error decoding and logging

/**
 * @brief Communication message structure for ESP-NOW
 * 
 * This structure is used for all communications between Master and Slave.
 * Both devices must use an identical structure to ensure compatibility.
 */
struct MessageSlave
{
  float measurement;       /**< Measurement value in mm */
  float batteryVoltage;    /**< Battery voltage in voltage */
  CommandType command;     /**< Command type */
  uint8_t angleX;            /**< Angle X from accelerometer ADXL345 */
};

struct MessageMaster
{
  uint32_t timeout;      /**< Timeout for run motor while measure (ms) */
  CommandType command;     /**< Command type */
  MotorState motorState;  /**< Current motor state */
  uint8_t motorSpeed;    /**< Motor speed (PWM value 0-255) */
  uint8_t motorTorque;   /**< Motor torque (PWM value 0-255) */
};

#ifdef CALIPER_MASTER
/**
 * @brief System status structure (Master only)
 * 
 * This structure tracks the overall system state on the Master device.
 */
struct SystemStatus
{
  struct MessageSlave msgSlave;
  struct MessageMaster msgMaster;

  // Offset kalibracji utrzymywany lokalnie na Master.
  // UI (WWW/GUI) wysyła go osobno, a korekcja jest liczona po stronie klienta:
  // corrected = msgSlave.measurement + calibrationOffset
  float calibrationOffset;

  // Nazwa sesji pomiarowej (maks 31 znaków + null terminator)
  // Ustawiana przez komendę 'n' lub przez WWW/GUI
  char sessionName[32];
};

#endif // CALIPER_MASTER

#endif // SHARED_COMMON_H
