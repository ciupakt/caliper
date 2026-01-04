/**
 * @file error_codes.h
 * @brief Comprehensive Error Code System for ESP32 Caliper System
 * @author System Generated
 * @date 2026-01-04
 * @version 1.0
 * 
 * This file provides a standardized error handling system with:
 * - Error categories (8 categories)
 * - Source modules (10 modules)
 * - Detailed error codes with recovery actions
 * - Helper functions for error decoding and logging
 * 
 * Error Code Format (16 bits):
 * [Category:4 bits][Module:4 bits][Code:8 bits]
 * 
 * Example: 0x0102 = Category:0x01 (Communication), Module:0x01 (ESP-NOW), Code:0x02 (Send Failed)
 */

#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include <stdint.h>

// ============================================================================
// Error Categories
// ============================================================================

/**
 * @brief Error categories for classification
 */
enum ErrorCategory : uint8_t
{
  ERR_CAT_NONE = 0,           /**< No error */
  ERR_CAT_COMMUNICATION,      /**< Communication errors (ESP-NOW, Serial, WiFi) */
  ERR_CAT_SENSOR,             /**< Sensor errors (caliper, accelerometer) */
  ERR_CAT_MOTOR,              /**< Motor controller errors */
  ERR_CAT_POWER,              /**< Power/Battery errors */
  ERR_CAT_STORAGE,            /**< Storage errors (LittleFS, NVS/Preferences) */
  ERR_CAT_NETWORK,            /**< Network errors (WiFi AP, Web Server) */
  ERR_CAT_VALIDATION,         /**< Data validation errors */
  ERR_CAT_SYSTEM              /**< System-level errors */
};

// ============================================================================
// Error Modules
// ============================================================================

/**
 * @brief Source modules that can generate errors
 */
enum ErrorModule : uint8_t
{
  ERR_MOD_NONE = 0,           /**< No specific module */
  ERR_MOD_ESPNOW,             /**< ESP-NOW communication */
  ERR_MOD_SERIAL,             /**< Serial communication */
  ERR_MOD_CALIPER,            /**< Caliper sensor */
  ERR_MOD_ACCELEROMETER,      /**< Accelerometer sensor */
  ERR_MOD_MOTOR_CTRL,         /**< Motor controller */
  ERR_MOD_BATTERY,            /**< Battery monitor */
  ERR_MOD_LITTLEFS,           /**< LittleFS file system */
  ERR_MOD_PREFERENCES,        /**< Preferences/NVS storage */
  ERR_MOD_WEB_SERVER,         /**< Web server */
  ERR_MOD_CLI                 /**< Serial CLI */
};

// ============================================================================
// Error Codes
// ============================================================================

/**
 * @brief Comprehensive error codes
 * 
 * Format: ERR_CAT_XXX | (ERR_MOD_XXX << 4) | (Code << 8)
 * 
 * Legend:
 * - 0x00XX: No error
 * - 0x01XX: Communication errors
 * - 0x02XX: Sensor errors
 * - 0x03XX: Motor errors
 * - 0x04XX: Power errors
 * - 0x05XX: Storage errors
 * - 0x06XX: Network errors
 * - 0x07XX: Validation errors
 * - 0x08XX: System errors
 */
enum ErrorCode : uint16_t
{
  // ============================================================================
  // No Error (0x0000)
  // ============================================================================
  ERR_NONE = 0x0000, /**< No error */

  // ============================================================================
  // Communication Errors (0x01XX)
  // ============================================================================
  /** ESP-NOW initialization failed */
  ERR_ESPNOW_INIT_FAILED = 0x0101,
  
  /** ESP-NOW send operation failed */
  ERR_ESPNOW_SEND_FAILED = 0x0102,
  
  /** ESP-NOW receive operation failed */
  ERR_ESPNOW_RECV_FAILED = 0x0103,
  
  /** ESP-NOW peer addition failed */
  ERR_ESPNOW_PEER_ADD_FAILED = 0x0104,
  
  /** ESP-NOW invalid packet length */
  ERR_ESPNOW_INVALID_LENGTH = 0x0105,
  
  /** Serial communication error */
  ERR_SERIAL_COMM_ERROR = 0x0106,
  
  /** Serial timeout */
  ERR_SERIAL_TIMEOUT = 0x0107,

  // ============================================================================
  // Sensor Errors (0x02XX)
  // ============================================================================
  /** Caliper measurement timeout */
  ERR_CALIPER_TIMEOUT = 0x0201,
  
  /** Caliper invalid data received */
  ERR_CALIPER_INVALID_DATA = 0x0202,
  
  /** Caliper measurement out of range */
  ERR_CALIPER_OUT_OF_RANGE = 0x0203,
  
  /** Caliper hardware failure */
  ERR_CALIPER_HARDWARE_FAILURE = 0x0204,
  
  /** Accelerometer initialization failed */
  ERR_ACCEL_INIT_FAILED = 0x0205,
  
  /** Accelerometer read failed */
  ERR_ACCEL_READ_FAILED = 0x0206,
  
  /** Accelerometer I2C communication error */
  ERR_ACCEL_I2C_ERROR = 0x0207,

  // ============================================================================
  // Motor Errors (0x03XX)
  // ============================================================================
  /** Motor invalid direction specified */
  ERR_MOTOR_INVALID_DIRECTION = 0x0301,
  
  /** Motor hardware failure detected */
  ERR_MOTOR_HARDWARE_FAILURE = 0x0302,

  // ============================================================================
  // Power Errors (0x04XX)
  // ============================================================================
  /** Battery voltage read failed */
  ERR_BATTERY_READ_FAILED = 0x0401,
  
  /** Battery voltage too low */
  ERR_BATTERY_LOW_VOLTAGE = 0x0402,
  
  /** ADC read error */
  ERR_ADC_READ_FAILED = 0x0403,

  // ============================================================================
  // Storage Errors (0x05XX)
  // ============================================================================
  /** LittleFS mount failed */
  ERR_LITTLEFS_MOUNT_FAILED = 0x0501,
  
  /** LittleFS file not found */
  ERR_LITTLEFS_FILE_NOT_FOUND = 0x0502,
  
  /** LittleFS read operation failed */
  ERR_LITTLEFS_READ_FAILED = 0x0503,
  
  /** LittleFS write operation failed */
  ERR_LITTLEFS_WRITE_FAILED = 0x0504,
  
  /** Preferences/NVS initialization failed */
  ERR_PREFS_INIT_FAILED = 0x0505,
  
  /** Preferences load failed */
  ERR_PREFS_LOAD_FAILED = 0x0506,
  
  /** Preferences save failed */
  ERR_PREFS_SAVE_FAILED = 0x0507,
  
  /** Preferences invalid value */
  ERR_PREFS_INVALID_VALUE = 0x0508,

  // ============================================================================
  // Network Errors (0x06XX)
  // ============================================================================
  /** Web server initialization failed */
  ERR_WEB_SERVER_INIT_FAILED = 0x0601,
  
  /** Web server route handler failed */
  ERR_WEB_SERVER_ROUTE_FAILED = 0x0602,
  
  /** WiFi initialization failed */
  ERR_WIFI_INIT_FAILED = 0x0603,
  
  /** WiFi AP configuration failed */
  ERR_WIFI_AP_CONFIG_FAILED = 0x0604,

  // ============================================================================
  // Validation Errors (0x07XX)
  // ============================================================================
  /** Invalid parameter provided */
  ERR_VALIDATION_INVALID_PARAM = 0x0701,
  
  /** Value out of valid range */
  ERR_VALIDATION_OUT_OF_RANGE = 0x0702,
  
  /** Invalid data format */
  ERR_VALIDATION_INVALID_FORMAT = 0x0703,
  
  /** Session not active */
  ERR_VALIDATION_SESSION_INACTIVE = 0x0704,
  
  /** Invalid command received */
  ERR_VALIDATION_INVALID_COMMAND = 0x0705,

  // ============================================================================
  // System Errors (0x08XX)
  // ============================================================================
  /** WiFi initialization failed */
  ERR_SYSTEM_WIFI_INIT_FAILED = 0x0801,
  
  /** Memory allocation failed */
  ERR_SYSTEM_MEMORY_ALLOC_FAILED = 0x0802,
  
  /** Unknown system error */
  ERR_SYSTEM_UNKNOWN_ERROR = 0x0803,
  
  /** Null pointer reference */
  ERR_SYSTEM_NULL_POINTER = 0x0804
};

// ============================================================================
// Error Helper Functions
// ============================================================================

/**
 * @brief Extract category from error code
 * @param code Error code to decode
 * @return Error category
 */
inline ErrorCategory getErrorCategory(ErrorCode code)
{
  return static_cast<ErrorCategory>(code & 0x0F);
}

/**
 * @brief Extract module from error code
 * @param code Error code to decode
 * @return Error module
 */
inline ErrorModule getErrorModule(ErrorCode code)
{
  return static_cast<ErrorModule>((code >> 4) & 0x0F);
}

/**
 * @brief Extract specific error code from error code
 * @param code Error code to decode
 * @return Specific error code (8 bits)
 */
inline uint8_t getErrorCode(ErrorCode code)
{
  return static_cast<uint8_t>((code >> 8) & 0xFF);
}

/**
 * @brief Get category name as string
 * @param cat Error category
 * @return Category name string
 */
const char* getErrorCategoryName(ErrorCategory cat);

/**
 * @brief Get module name as string
 * @param mod Error module
 * @return Module name string
 */
const char* getErrorModuleName(ErrorModule mod);

/**
 * @brief Get error description
 * @param code Error code
 * @return Error description string
 */
const char* getErrorDescription(ErrorCode code);

/**
 * @brief Get recovery action suggestion
 * @param code Error code
 * @return Recovery action string
 */
const char* getErrorRecoveryAction(ErrorCode code);

/**
 * @brief Check if error code is valid
 * @param code Error code to check
 * @return true if valid, false otherwise
 */
inline bool isValidErrorCode(ErrorCode code)
{
  if (code == ERR_NONE)
    return true;
  
  ErrorCategory cat = getErrorCategory(code);
  if (cat < ERR_CAT_NONE || cat > ERR_CAT_SYSTEM)
    return false;
  
  ErrorModule mod = getErrorModule(code);
  if (mod < ERR_MOD_NONE || mod > ERR_MOD_CLI)
    return false;
  
  return true;
}

/**
 * @brief Check if error is recoverable
 * @param code Error code to check
 * @return true if recoverable, false otherwise
 */
bool isRecoverableError(ErrorCode code);

/**
 * @brief Get error severity level
 * @param code Error code
 * @return Severity level (0=info, 1=warning, 2=error, 3=critical)
 */
uint8_t getErrorSeverity(ErrorCode code);

#endif // ERROR_CODES_H
