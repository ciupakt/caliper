/**
 * @file preferences_manager.h
 * @brief Preferences Manager for ESP32 Caliper Master
 * @author System Generated
 * @date 2025-12-26
 * @version 2.0
 *
 * This module provides persistent storage for caliper settings using ESP32 Preferences library.
 * Settings are stored in NVS (Non-Volatile Storage) and persist across reboots.
 *
 * @version 2.0 - Integrated comprehensive error code system
 */

#ifndef PREFERENCES_MANAGER_H
#define PREFERENCES_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <shared_common.h>
#include <error_handler.h>

/**
 * @brief Preferences Manager class for persistent storage
 * 
 * This class handles loading and saving caliper settings to ESP32 NVS.
 * Settings are automatically loaded on startup and can be saved individually.
 */
class PreferencesManager
{
public:
  /**
   * @brief Default constructor
   */
  PreferencesManager();

  /**
   * @brief Initialize Preferences library and load settings
   * 
   * Opens the NVS namespace and loads saved settings.
   * If no settings are found, default values are used.
   * 
   * @return true if initialization successful, false otherwise
   */
  bool begin();

  /**
   * @brief Load settings from NVS to SystemStatus
   * 
   * Loads all settings from NVS and updates the SystemStatus structure.
   * If a setting is not found in NVS, the default value is used.
   * 
   * @param status Pointer to SystemStatus structure to update
   */
  void loadSettings(SystemStatus *status);

  /**
   * @brief Save motorSpeed to NVS
   *
   * Possible errors:
   * - ERR_PREFS_SAVE_FAILED: Save operation failed
   * - ERR_VALIDATION_OUT_OF_RANGE: Value out of valid range
   *
   * @param value Motor speed value (0-255)
   */
  void saveMotorSpeed(uint8_t value);

  /**
   * @brief Save motorTorque to NVS
   *
   * Possible errors:
   * - ERR_PREFS_SAVE_FAILED: Save operation failed
   * - ERR_VALIDATION_OUT_OF_RANGE: Value out of valid range
   *
   * @param value Motor torque value (0-255)
   */
  void saveMotorTorque(uint8_t value);

  /**
   * @brief Save timeout to NVS
   *
   * Possible errors:
   * - ERR_PREFS_SAVE_FAILED: Save operation failed
   * - ERR_VALIDATION_OUT_OF_RANGE: Value out of valid range
   *
   * @param value Timeout value in milliseconds (0-600000)
   */
  void saveTimeout(uint32_t value);

  /**
   * @brief Save calibrationOffset to NVS
   *
   * Possible errors:
   * - ERR_PREFS_SAVE_FAILED: Save operation failed
   * - ERR_VALIDATION_OUT_OF_RANGE: Value out of valid range
   *
   * @param value Calibration offset in mm (-14.999..14.999)
   */
  void saveCalibrationOffset(float value);

  /**
   * @brief Reset all settings to default values
   * 
   * Clears all settings from NVS and resets to defaults.
   * Default values:
   * - motorSpeed: 100
   * - motorTorque: 100
   * - timeout: 1000 ms
   * - calibrationOffset: 0.0 mm
   */
  void resetToDefaults();

  /**
   * @brief Check if settings are valid
   *
   * @return true if settings are valid, false otherwise
   */
  bool isSettingsValid();

private:
  Preferences prefs; ///< Preferences instance

  // Namespace and key names
  static constexpr const char *NAMESPACE = "caliper_config";
  static constexpr const char *KEY_MOTOR_SPEED = "motorSpeed";
  static constexpr const char *KEY_MOTOR_TORQUE = "motorTorque";
  static constexpr const char *KEY_TIMEOUT = "timeout";
  static constexpr const char *KEY_CALIBRATION_OFFSET = "calibrationOffset";

  // Default values
  static constexpr uint8_t DEFAULT_MOTOR_SPEED = 100;
  static constexpr uint8_t DEFAULT_MOTOR_TORQUE = 100;
  static constexpr uint32_t DEFAULT_TIMEOUT_MS = 1000;
  static constexpr float DEFAULT_CALIBRATION_OFFSET = 0.0f;

  // Value ranges
  static constexpr uint8_t MIN_MOTOR_SPEED = 0;
  static constexpr uint8_t MAX_MOTOR_SPEED = 255;
  static constexpr uint8_t MIN_MOTOR_TORQUE = 0;
  static constexpr uint8_t MAX_MOTOR_TORQUE = 255;
  static constexpr uint32_t MIN_TIMEOUT_MS = 0;
  static constexpr uint32_t MAX_TIMEOUT_MS = 600000;
  static constexpr float MIN_CALIBRATION_OFFSET = -14.999f;
  static constexpr float MAX_CALIBRATION_OFFSET = 14.999f;

  /**
   * @brief Validate motorSpeed value
   * 
   * @param value Value to validate
   * @return true if valid, false otherwise
   */
  bool validateMotorSpeed(uint8_t value) const;

  /**
   * @brief Validate motorTorque value
   * 
   * @param value Value to validate
   * @return true if valid, false otherwise
   */
  bool validateMotorTorque(uint8_t value) const;

  /**
   * @brief Validate timeout value
   * 
   * @param value Value to validate
   * @return true if valid, false otherwise
   */
  bool validateTimeout(uint32_t value) const;

  /**
   * @brief Validate calibrationOffset value
   * 
   * @param value Value to validate
   * @return true if valid, false otherwise
   */
  bool validateCalibrationOffset(float value) const;
};

#endif // PREFERENCES_MANAGER_H
