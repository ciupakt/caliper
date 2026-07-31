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

#define LOG_ERROR(code, ...) \
  do { \
    DEBUG_E("[ERROR] %s:%s - Code:0x%04X", \
      getErrorCategoryName(code), \
      getErrorModuleName(code), \
      (unsigned)code); \
    DEBUG_E("  Description: %s", getErrorDescription(code)); \
    DEBUG_E("  Recovery: %s", getErrorRecoveryAction(code)); \
    if (sizeof(#__VA_ARGS__) > 1) { \
      DEBUG_E("  Details: " __VA_ARGS__); \
    } \
  } while (0)

#define LOG_WARNING(code, ...) \
  do { \
    DEBUG_W("[WARNING] %s:%s - Code:0x%04X", \
      getErrorCategoryName(code), \
      getErrorModuleName(code), \
      (unsigned)code); \
    DEBUG_W("  Description: %s", getErrorDescription(code)); \
    if (sizeof(#__VA_ARGS__) > 1) { \
      DEBUG_W("  Details: " __VA_ARGS__); \
    } \
  } while (0)

// ============================================================================
// Error Handler Class
// ============================================================================

class ErrorHandler
{
public:
  static ErrorHandler& getInstance()
  {
    static ErrorHandler instance;
    return instance;
  }

  void initialize()
  {
    stats.totalErrors = 0;
    stats.recoverableErrors = 0;
    stats.criticalErrors = 0;
    stats.lastErrorTime = 0;
    stats.lastErrorCode = ERR_NONE;
  }

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

  const ErrorStats& getStats() const
  {
    return stats;
  }

  void resetStats()
  {
    stats.totalErrors = 0;
    stats.recoverableErrors = 0;
    stats.criticalErrors = 0;
    stats.lastErrorTime = 0;
    stats.lastErrorCode = ERR_NONE;
  }

  ErrorCode getLastError() const
  {
    return stats.lastErrorCode;
  }

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
  ErrorHandler() = default;
  ErrorHandler(const ErrorHandler&) = delete;
  ErrorHandler& operator=(const ErrorHandler&) = delete;
};

// ============================================================================
// Convenience Macros for Error Handler
// ============================================================================

#define ERROR_HANDLER ErrorHandler::getInstance()

#define RECORD_ERROR(code, ...) \
  do { \
    ERROR_HANDLER.recordError(code); \
    LOG_ERROR(code, __VA_ARGS__); \
  } while (0)

#endif // ERROR_HANDLER_H
