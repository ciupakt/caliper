/**
 * @file common.h
 * @brief Shared definitions and structures for ESP32 Caliper System
 * @author System Generated
 * @date 2025-12-26
 * @version 2.0
 * 
 * This is the unified common header file for both Master and Slave devices.
 * Use build flags to enable device-specific features:
 * - CALIPER_MASTER: Enables Master-specific structures
 * - CALIPER_SLAVE: Enables Slave-specific structures
 */

#ifndef SHARED_COMMON_H
#define SHARED_COMMON_H

#include <stdint.h>

/**
 * @brief Command types for ESP-NOW communication
 */
enum CommandType : char
{
  CMD_MEASURE = 'M', /**< Request measurement from slave */
  CMD_FORWARD = 'F', /**< Motor forward command */
  CMD_REVERSE = 'R', /**< Motor reverse command */
  CMD_STOP = 'S',    /**< Motor stop command */
  CMD_UPDATE = 'U'   /**< Status update */
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
 * @brief Error codes for system operations
 */
enum ErrorCode : uint8_t
{
  ERR_NONE = 0,             /**< No error */
  ERR_ESPNOW_SEND,          /**< ESP-NOW send failed */
  ERR_MEASUREMENT_TIMEOUT,  /**< Measurement timeout */
  ERR_INVALID_DATA,         /**< Invalid data received */
  ERR_ADC_READ,             /**< ADC read error */
  ERR_INVALID_COMMAND       /**< Invalid command received */
};

/**
 * @brief Communication message structure for ESP-NOW
 * 
 * This structure is used for all communications between Master and Slave.
 * Both devices must use an identical structure to ensure compatibility.
 */
struct Message
{
  float measurement;       /**< Measurement value in mm */
  bool valid;              /**< Whether measurement is valid */
  uint32_t timestamp;      /**< Timestamp from system start (ms) */
  CommandType command;     /**< Command type */
  uint16_t batteryVoltage; /**< Battery voltage in millivolts */
};

#ifdef CALIPER_MASTER
/**
 * @brief System status structure (Master only)
 * 
 * This structure tracks the overall system state on the Master device.
 */
struct SystemStatus
{
  float lastMeasurement;     /**< Last valid measurement */
  uint16_t batteryVoltage;   /**< Current battery voltage */
  bool motorRunning;         /**< Whether motor is running */
  MotorState motorDirection; /**< Current motor direction */
  uint32_t lastUpdate;       /**< Last update timestamp */
  bool measurementValid;     /**< Whether last measurement is valid */
  bool communicationActive;  /**< Whether ESP-NOW communication is active */
};
#endif // CALIPER_MASTER

#endif // SHARED_COMMON_H
