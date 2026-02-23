/**
 * @file error_codes.cpp
 * @brief Implementation of Error Code System Helper Functions
 * @author System Generated
 * @date 2026-01-04
 * @version 1.0
 */

#include "error_codes.h"

// ============================================================================
// Category Names
// ============================================================================

const char* getErrorCategoryName(ErrorCategory cat)
{
  switch (cat)
  {
    case ERR_CAT_NONE:
      return "NONE";
    case ERR_CAT_COMMUNICATION:
      return "COMMUNICATION";
    case ERR_CAT_SENSOR:
      return "SENSOR";
    case ERR_CAT_MOTOR:
      return "MOTOR";
    case ERR_CAT_POWER:
      return "POWER";
    case ERR_CAT_STORAGE:
      return "STORAGE";
    case ERR_CAT_NETWORK:
      return "NETWORK";
    case ERR_CAT_VALIDATION:
      return "VALIDATION";
    case ERR_CAT_SYSTEM:
      return "SYSTEM";
    default:
      return "UNKNOWN";
  }
}

// ============================================================================
// Module Names
// ============================================================================

const char* getErrorModuleName(ErrorModule mod)
{
  switch (mod)
  {
    case ERR_MOD_NONE:
      return "NONE";
    case ERR_MOD_ESPNOW:
      return "ESPNOW";
    case ERR_MOD_SERIAL:
      return "SERIAL";
    case ERR_MOD_CALIPER:
      return "CALIPER";
    case ERR_MOD_ACCELEROMETER:
      return "ACCELEROMETER";
    case ERR_MOD_MOTOR_CTRL:
      return "MOTOR_CTRL";
    case ERR_MOD_BATTERY:
      return "BATTERY";
    case ERR_MOD_LITTLEFS:
      return "LITTLEFS";
    case ERR_MOD_PREFERENCES:
      return "PREFERENCES";
    case ERR_MOD_WEB_SERVER:
      return "WEB_SERVER";
    case ERR_MOD_CLI:
      return "CLI";
    default:
      return "UNKNOWN";
  }
}

// ============================================================================
// Error Descriptions
// ============================================================================

const char* getErrorDescription(ErrorCode code)
{
  switch (code)
  {
    // No Error
    case ERR_NONE:
      return "No error";

    // Communication Errors
    case ERR_ESPNOW_INIT_FAILED:
      return "ESP-NOW initialization failed";
    case ERR_ESPNOW_SEND_FAILED:
      return "ESP-NOW send operation failed";
    case ERR_ESPNOW_RECV_FAILED:
      return "ESP-NOW receive operation failed";
    case ERR_ESPNOW_PEER_ADD_FAILED:
      return "ESP-NOW peer addition failed";
    case ERR_ESPNOW_INVALID_LENGTH:
      return "ESP-NOW invalid packet length";
    case ERR_SERIAL_COMM_ERROR:
      return "Serial communication error";
    case ERR_SERIAL_TIMEOUT:
      return "Serial operation timeout";

    // Sensor Errors
    case ERR_CALIPER_TIMEOUT:
      return "Caliper measurement timeout";
    case ERR_CALIPER_INVALID_DATA:
      return "Caliper invalid data received";
    case ERR_CALIPER_OUT_OF_RANGE:
      return "Caliper measurement out of range";
    case ERR_CALIPER_HARDWARE_FAILURE:
      return "Caliper hardware failure detected";
    case ERR_ACCEL_INIT_FAILED:
      return "Accelerometer initialization failed";
    case ERR_ACCEL_READ_FAILED:
      return "Accelerometer read operation failed";
    case ERR_ACCEL_I2C_ERROR:
      return "Accelerometer I2C communication error";

    // Motor Errors
    case ERR_MOTOR_INVALID_DIRECTION:
      return "Motor invalid direction specified";
    case ERR_MOTOR_HARDWARE_FAILURE:
      return "Motor hardware failure detected";
    case ERR_MOTOR_FAULT:
      return "Motor fault detected - overcurrent or thermal shutdown";

    // Power Errors
    case ERR_BATTERY_READ_FAILED:
      return "Battery voltage read failed";
    case ERR_BATTERY_LOW_VOLTAGE:
      return "Battery voltage too low";
    case ERR_ADC_READ_FAILED:
      return "ADC read operation failed";

    // Storage Errors
    case ERR_LITTLEFS_MOUNT_FAILED:
      return "LittleFS mount failed";
    case ERR_LITTLEFS_FILE_NOT_FOUND:
      return "LittleFS file not found";
    case ERR_LITTLEFS_READ_FAILED:
      return "LittleFS read operation failed";
    case ERR_LITTLEFS_WRITE_FAILED:
      return "LittleFS write operation failed";
    case ERR_PREFS_INIT_FAILED:
      return "Preferences/NVS initialization failed";
    case ERR_PREFS_LOAD_FAILED:
      return "Preferences load operation failed";
    case ERR_PREFS_SAVE_FAILED:
      return "Preferences save operation failed";
    case ERR_PREFS_INVALID_VALUE:
      return "Preferences invalid value";

    // Network Errors
    case ERR_WEB_SERVER_INIT_FAILED:
      return "Web server initialization failed";
    case ERR_WEB_SERVER_ROUTE_FAILED:
      return "Web server route handler failed";
    case ERR_WIFI_INIT_FAILED:
      return "WiFi initialization failed";
    case ERR_WIFI_AP_CONFIG_FAILED:
      return "WiFi AP configuration failed";

    // Validation Errors
    case ERR_VALIDATION_INVALID_PARAM:
      return "Invalid parameter provided";
    case ERR_VALIDATION_OUT_OF_RANGE:
      return "Value out of valid range";
    case ERR_VALIDATION_INVALID_FORMAT:
      return "Invalid data format";
    case ERR_VALIDATION_SESSION_INACTIVE:
      return "Measurement session not active";
    case ERR_VALIDATION_INVALID_COMMAND:
      return "Invalid command received";

    // System Errors
    case ERR_SYSTEM_WIFI_INIT_FAILED:
      return "System WiFi initialization failed";
    case ERR_SYSTEM_MEMORY_ALLOC_FAILED:
      return "Memory allocation failed";
    case ERR_SYSTEM_UNKNOWN_ERROR:
      return "Unknown system error";
    case ERR_SYSTEM_NULL_POINTER:
      return "Null pointer reference";

    default:
      return "Unknown error code";
  }
}

// ============================================================================
// Recovery Actions
// ============================================================================

const char* getErrorRecoveryAction(ErrorCode code)
{
  switch (code)
  {
    // No Error
    case ERR_NONE:
      return "No action required";

    // Communication Errors
    case ERR_ESPNOW_INIT_FAILED:
      return "Check WiFi module, restart device, verify ESP-NOW configuration";
    case ERR_ESPNOW_SEND_FAILED:
      return "Check peer connection, retry operation, verify signal strength";
    case ERR_ESPNOW_RECV_FAILED:
      return "Check receiver, verify peer is online, retry operation";
    case ERR_ESPNOW_PEER_ADD_FAILED:
      return "Verify MAC address, check WiFi channel, ensure both devices on same channel";
    case ERR_ESPNOW_INVALID_LENGTH:
      return "Check message structure, verify data integrity, update firmware if needed";
    case ERR_SERIAL_COMM_ERROR:
      return "Check serial connection, verify baud rate, restart device";
    case ERR_SERIAL_TIMEOUT:
      return "Check serial connection, verify baud rate, reduce data rate";

    // Sensor Errors
    case ERR_CALIPER_TIMEOUT:
      return "Check caliper connection, verify trigger pin, increase timeout value";
    case ERR_CALIPER_INVALID_DATA:
      return "Check caliper hardware, verify clock/data pins, restart measurement";
    case ERR_CALIPER_OUT_OF_RANGE:
      return "Verify measurement value, check caliper zero position, recalibrate if needed";
    case ERR_CALIPER_HARDWARE_FAILURE:
      return "Replace caliper, check wiring, verify power supply";
    case ERR_ACCEL_INIT_FAILED:
      return "Check I2C connection, verify IIS328DQ address, check power supply";
    case ERR_ACCEL_READ_FAILED:
      return "Retry read operation, check I2C connection, verify sensor power";
    case ERR_ACCEL_I2C_ERROR:
      return "Check I2C wiring, verify pull-up resistors, restart I2C bus";

    // Motor Errors
    case ERR_MOTOR_INVALID_DIRECTION:
      return "Use valid direction (STOP, FORWARD, REVERSE, BRAKE), check motor state";
    case ERR_MOTOR_HARDWARE_FAILURE:
      return "Check motor connections, verify STSPIN250 driver, check power supply";
    case ERR_MOTOR_FAULT:
      return "Check for short circuit, allow motor to cool, reduce load or current limit";

    // Power Errors
    case ERR_BATTERY_READ_FAILED:
      return "Check ADC configuration, verify voltage divider, restart device";
    case ERR_BATTERY_LOW_VOLTAGE:
      return "Charge or replace battery, check power supply connections";
    case ERR_ADC_READ_FAILED:
      return "Check ADC configuration, verify pin assignment, restart device";

    // Storage Errors
    case ERR_LITTLEFS_MOUNT_FAILED:
      return "Format LittleFS, check flash memory, restart device";
    case ERR_LITTLEFS_FILE_NOT_FOUND:
      return "Verify file exists, check file path, reupload files if needed";
    case ERR_LITTLEFS_READ_FAILED:
      return "Check file integrity, verify file permissions, retry read operation";
    case ERR_LITTLEFS_WRITE_FAILED:
      return "Check available space, verify write permissions, retry write operation";
    case ERR_PREFS_INIT_FAILED:
      return "Check NVS partition, erase NVS if corrupted, restart device";
    case ERR_PREFS_LOAD_FAILED:
      return "Reset to defaults, check NVS integrity, restart device";
    case ERR_PREFS_SAVE_FAILED:
      return "Check available NVS space, verify value validity, retry save operation";
    case ERR_PREFS_INVALID_VALUE:
      return "Use valid value range, reset to defaults, verify configuration";

    // Network Errors
    case ERR_WEB_SERVER_INIT_FAILED:
      return "Check port availability, restart device, verify web server configuration";
    case ERR_WEB_SERVER_ROUTE_FAILED:
      return "Check route handler, verify endpoint configuration, restart web server";
    case ERR_WIFI_INIT_FAILED:
      return "Check WiFi module, restart device, verify WiFi configuration";
    case ERR_WIFI_AP_CONFIG_FAILED:
      return "Check SSID/password, verify AP settings, restart WiFi";

    // Validation Errors
    case ERR_VALIDATION_INVALID_PARAM:
      return "Check parameter value, verify input format, consult documentation";
    case ERR_VALIDATION_OUT_OF_RANGE:
      return "Use value within valid range, check min/max limits, adjust input";
    case ERR_VALIDATION_INVALID_FORMAT:
      return "Check input format, verify data type, use correct format";
    case ERR_VALIDATION_SESSION_INACTIVE:
      return "Start a new session, verify session name, check session status";
    case ERR_VALIDATION_INVALID_COMMAND:
      return "Use valid command, check command syntax, consult documentation";

    // System Errors
    case ERR_SYSTEM_WIFI_INIT_FAILED:
      return "Restart device, check WiFi hardware, update firmware";
    case ERR_SYSTEM_MEMORY_ALLOC_FAILED:
      return "Reduce memory usage, restart device, check for memory leaks";
    case ERR_SYSTEM_UNKNOWN_ERROR:
      return "Restart device, check logs, contact support if persists";
    case ERR_SYSTEM_NULL_POINTER:
      return "Check code for null references, verify pointer initialization, debug code";

    default:
      return "Unknown error - check logs and contact support";
  }
}

// ============================================================================
// Error Recoverability
// ============================================================================

bool isRecoverableError(ErrorCode code)
{
  switch (code)
  {
    // Always recoverable
    case ERR_NONE:
    case ERR_ESPNOW_SEND_FAILED:
    case ERR_ESPNOW_RECV_FAILED:
    case ERR_SERIAL_COMM_ERROR:
    case ERR_SERIAL_TIMEOUT:
    case ERR_CALIPER_TIMEOUT:
    case ERR_CALIPER_INVALID_DATA:
    case ERR_CALIPER_OUT_OF_RANGE:
    case ERR_ACCEL_READ_FAILED:
    case ERR_ACCEL_I2C_ERROR:
    case ERR_MOTOR_INVALID_DIRECTION:
    case ERR_BATTERY_READ_FAILED:
    case ERR_ADC_READ_FAILED:
    case ERR_LITTLEFS_FILE_NOT_FOUND:
    case ERR_LITTLEFS_READ_FAILED:
    case ERR_LITTLEFS_WRITE_FAILED:
    case ERR_PREFS_LOAD_FAILED:
    case ERR_PREFS_SAVE_FAILED:
    case ERR_PREFS_INVALID_VALUE:
    case ERR_WEB_SERVER_ROUTE_FAILED:
    case ERR_VALIDATION_INVALID_PARAM:
    case ERR_VALIDATION_OUT_OF_RANGE:
    case ERR_VALIDATION_INVALID_FORMAT:
    case ERR_VALIDATION_SESSION_INACTIVE:
    case ERR_VALIDATION_INVALID_COMMAND:
    case ERR_SYSTEM_MEMORY_ALLOC_FAILED:
    case ERR_SYSTEM_UNKNOWN_ERROR:
      return true;

    // May require intervention
    case ERR_ESPNOW_INIT_FAILED:
    case ERR_ESPNOW_PEER_ADD_FAILED:
    case ERR_ESPNOW_INVALID_LENGTH:
    case ERR_ACCEL_INIT_FAILED:
    case ERR_BATTERY_LOW_VOLTAGE:
    case ERR_LITTLEFS_MOUNT_FAILED:
    case ERR_PREFS_INIT_FAILED:
    case ERR_WEB_SERVER_INIT_FAILED:
    case ERR_WIFI_INIT_FAILED:
    case ERR_WIFI_AP_CONFIG_FAILED:
    case ERR_SYSTEM_WIFI_INIT_FAILED:
    case ERR_SYSTEM_NULL_POINTER:
      return false;

    // Hardware failures - not recoverable
    case ERR_CALIPER_HARDWARE_FAILURE:
    case ERR_MOTOR_HARDWARE_FAILURE:
    case ERR_MOTOR_FAULT:
      return false;

    default:
      return false;
  }
}

// ============================================================================
// Error Severity Levels
// ============================================================================

uint8_t getErrorSeverity(ErrorCode code)
{
  switch (code)
  {
    // Info level (0)
    case ERR_NONE:
      return 0;

    // Warning level (1)
    case ERR_ESPNOW_SEND_FAILED:
    case ERR_ESPNOW_RECV_FAILED:
    case ERR_SERIAL_TIMEOUT:
    case ERR_CALIPER_TIMEOUT:
    case ERR_ACCEL_READ_FAILED:
    case ERR_ACCEL_I2C_ERROR:
    case ERR_BATTERY_LOW_VOLTAGE:
    case ERR_LITTLEFS_FILE_NOT_FOUND:
    case ERR_PREFS_LOAD_FAILED:
    case ERR_PREFS_INVALID_VALUE:
    case ERR_VALIDATION_INVALID_PARAM:
    case ERR_VALIDATION_OUT_OF_RANGE:
    case ERR_VALIDATION_INVALID_FORMAT:
    case ERR_VALIDATION_SESSION_INACTIVE:
      return 1;

    // Error level (2)
    case ERR_ESPNOW_PEER_ADD_FAILED:
    case ERR_ESPNOW_INVALID_LENGTH:
    case ERR_SERIAL_COMM_ERROR:
    case ERR_CALIPER_INVALID_DATA:
    case ERR_CALIPER_OUT_OF_RANGE:
    case ERR_ACCEL_INIT_FAILED:
    case ERR_MOTOR_INVALID_DIRECTION:
    case ERR_BATTERY_READ_FAILED:
    case ERR_ADC_READ_FAILED:
    case ERR_LITTLEFS_READ_FAILED:
    case ERR_LITTLEFS_WRITE_FAILED:
    case ERR_PREFS_SAVE_FAILED:
    case ERR_WEB_SERVER_ROUTE_FAILED:
    case ERR_WIFI_AP_CONFIG_FAILED:
    case ERR_VALIDATION_INVALID_COMMAND:
    case ERR_SYSTEM_MEMORY_ALLOC_FAILED:
    case ERR_SYSTEM_UNKNOWN_ERROR:
      return 2;

    // Critical level (3)
    case ERR_ESPNOW_INIT_FAILED:
    case ERR_CALIPER_HARDWARE_FAILURE:
    case ERR_MOTOR_HARDWARE_FAILURE:
    case ERR_MOTOR_FAULT:
    case ERR_LITTLEFS_MOUNT_FAILED:
    case ERR_PREFS_INIT_FAILED:
    case ERR_WEB_SERVER_INIT_FAILED:
    case ERR_WIFI_INIT_FAILED:
    case ERR_SYSTEM_WIFI_INIT_FAILED:
    case ERR_SYSTEM_NULL_POINTER:
      return 3;

    default:
      return 2;
  }
}
