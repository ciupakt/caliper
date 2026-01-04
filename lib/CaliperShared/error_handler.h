/**
 * @file error_handler.h
 * @brief Error Handler System with Logging Macros
 * @author System Generated
 * @date 2026-01-04
 * @version 1.0
 * 
 * This file provides a comprehensive error handling system with:
 * - Error logging macros with automatic decoding
 * - Error tracking and statistics
 * - Error context management
 * - Integration with MacroDebugger
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include "error_codes.h"
#include <MacroDebugger.h>

// ============================================================================
// Error Statistics Structure
// ============================================================================

/**
 * @brief Error statistics tracking
 */
struct ErrorStats
{
  uint32_t totalErrors;      /**< Total number of errors */
  uint32_t recoverableErrors; /**< Number of recoverable errors */
  uint32_t criticalErrors;    /**< Number of critical errors */
  uint32_t lastErrorTime;    /**< Timestamp of last error (ms) */
  ErrorCode lastErrorCode;    /**< Last error code */
};

// ============================================================================
// Error Logging Macros
// ============================================================================

/**
 * @brief Log error with full details
 * 
 * This macro logs:
 * - Error category and module names
 * - Error code (hex)
 * - Error description
 * - Recovery action
 * - Custom message (optional)
 * 
 * @param code Error code to log
 * @param ... Optional custom message with printf-style formatting
 */
#define LOG_ERROR(code, ...) \
  do { \
    DEBUG_E("[ERROR] %s:%s - Code:0x%04X", \
      getErrorCategoryName(getErrorCategory(code)), \
      getErrorModuleName(getErrorModule(code)), \
      (unsigned)code); \
    DEBUG_E("  Description: %s", getErrorDescription(code)); \
    DEBUG_E("  Recovery: %s", getErrorRecoveryAction(code)); \
    if (sizeof(#__VA_ARGS__) > 1) { \
      DEBUG_E("  Details: " __VA_ARGS__); \
    } \
  } while (0)

/**
 * @brief Log warning with error details
 * 
 * @param code Error code to log
 * @param ... Optional custom message with printf-style formatting
 */
#define LOG_WARNING(code, ...) \
  do { \
    DEBUG_W("[WARNING] %s:%s - Code:0x%04X", \
      getErrorCategoryName(getErrorCategory(code)), \
      getErrorModuleName(getErrorModule(code)), \
      (unsigned)code); \
    DEBUG_W("  Description: %s", getErrorDescription(code)); \
    if (sizeof(#__VA_ARGS__) > 1) { \
      DEBUG_W("  Details: " __VA_ARGS__); \
    } \
  } while (0)

/**
 * @brief Log info with error details
 * 
 * @param code Error code to log
 * @param ... Optional custom message with printf-style formatting
 */
#define LOG_INFO(code, ...) \
  do { \
    DEBUG_I("[INFO] %s:%s - Code:0x%04X", \
      getErrorCategoryName(getErrorCategory(code)), \
      getErrorModuleName(getErrorModule(code)), \
      (unsigned)code); \
    DEBUG_I("  Description: %s", getErrorDescription(code)); \
    if (sizeof(#__VA_ARGS__) > 1) { \
      DEBUG_I("  Details: " __VA_ARGS__); \
    } \
  } while (0)

/**
 * @brief Log error and return error code
 * 
 * Use this macro when you want to log an error and immediately return.
 * 
 * @param code Error code to log and return
 * @param ... Optional custom message with printf-style formatting
 */
#define RETURN_ERROR(code, ...) \
  do { \
    LOG_ERROR(code, __VA_ARGS__); \
    return code; \
  } while (0)

/**
 * @brief Log error and return if condition is true
 * 
 * Use this macro for error checking with conditional return.
 * 
 * @param condition Condition to check
 * @param code Error code to log and return if condition is true
 * @param ... Optional custom message with printf-style formatting
 */
#define RETURN_IF_ERROR(condition, code, ...) \
  do { \
    if (condition) { \
      RETURN_ERROR(code, __VA_ARGS__); \
    } \
  } while (0)

/**
 * @brief Log error and return if error code is not ERR_NONE
 * 
 * Use this macro for checking function return values.
 * 
 * @param errorCode Error code to check
 * @param ... Optional custom message with printf-style formatting
 */
#define RETURN_IF_NOT_OK(errorCode, ...) \
  do { \
    if (errorCode != ERR_NONE) { \
      RETURN_ERROR(errorCode, __VA_ARGS__); \
    } \
  } while (0)

/**
 * @brief Log warning if condition is true
 * 
 * @param condition Condition to check
 * @param code Error code to log if condition is true
 * @param ... Optional custom message with printf-style formatting
 */
#define WARN_IF(condition, code, ...) \
  do { \
    if (condition) { \
      LOG_WARNING(code, __VA_ARGS__); \
    } \
  } while (0)

/**
 * @brief Assert condition and log error if false
 * 
 * This macro is similar to standard assert() but uses the error logging system.
 * 
 * @param condition Condition to check
 * @param code Error code to log if condition is false
 * @param ... Optional custom message with printf-style formatting
 */
#define ERROR_ASSERT(condition, code, ...) \
  do { \
    if (!(condition)) { \
      LOG_ERROR(code, "Assertion failed: " #condition, __VA_ARGS__); \
    } \
  } while (0)

// ============================================================================
// Error Handler Class
// ============================================================================

/**
 * @brief Error handler class for tracking and managing errors
 */
class ErrorHandler
{
public:
  /**
   * @brief Get singleton instance
   * @return Reference to error handler instance
   */
  static ErrorHandler& getInstance()
  {
    static ErrorHandler instance;
    return instance;
  }

  /**
   * @brief Initialize error handler
   */
  void initialize()
  {
    stats.totalErrors = 0;
    stats.recoverableErrors = 0;
    stats.criticalErrors = 0;
    stats.lastErrorTime = 0;
    stats.lastErrorCode = ERR_NONE;
  }

  /**
   * @brief Record an error
   * @param code Error code to record
   */
  void recordError(ErrorCode code)
  {
    stats.totalErrors++;
    stats.lastErrorCode = code;
    stats.lastErrorTime = millis();

    if (isRecoverableError(code))
    {
      stats.recoverableErrors++;
    }

    if (getErrorSeverity(code) >= 3)
    {
      stats.criticalErrors++;
    }
  }

  /**
   * @brief Get error statistics
   * @return Reference to error statistics structure
   */
  const ErrorStats& getStats() const
  {
    return stats;
  }

  /**
   * @brief Reset error statistics
   */
  void resetStats()
  {
    stats.totalErrors = 0;
    stats.recoverableErrors = 0;
    stats.criticalErrors = 0;
    stats.lastErrorTime = 0;
    stats.lastErrorCode = ERR_NONE;
  }

  /**
   * @brief Get last error code
   * @return Last error code
   */
  ErrorCode getLastError() const
  {
    return stats.lastErrorCode;
  }

  /**
   * @brief Get time since last error
   * @return Time in milliseconds since last error
   */
  uint32_t getTimeSinceLastError() const
  {
    if (stats.lastErrorTime == 0)
    {
      return 0;
    }
    return millis() - stats.lastErrorTime;
  }

private:
  ErrorStats stats;

  // Private constructor for singleton
  ErrorHandler() = default;
  ErrorHandler(const ErrorHandler&) = delete;
  ErrorHandler& operator=(const ErrorHandler&) = delete;
};

// ============================================================================
// Convenience Macros for Error Handler
// ============================================================================

/**
 * @brief Get error handler instance
 */
#define ERROR_HANDLER ErrorHandler::getInstance()

/**
 * @brief Record error and log it
 * @param code Error code to record and log
 * @param ... Optional custom message
 */
#define RECORD_ERROR(code, ...) \
  do { \
    ERROR_HANDLER.recordError(code); \
    LOG_ERROR(code, __VA_ARGS__); \
  } while (0)

/**
 * @brief Record error, log it, and return
 * @param code Error code to record, log, and return
 * @param ... Optional custom message
 */
#define RECORD_AND_RETURN_ERROR(code, ...) \
  do { \
    RECORD_ERROR(code, __VA_ARGS__); \
    return code; \
  } while (0)

#endif // ERROR_HANDLER_H
