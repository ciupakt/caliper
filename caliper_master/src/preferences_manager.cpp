/**
 * @file preferences_manager.cpp
 * @brief Preferences Manager implementation for ESP32 Caliper Master
 * @author System Generated
 * @date 2025-12-26
 * @version 1.0
 */

#include "preferences_manager.h"
#include <MacroDebugger.h>

PreferencesManager::PreferencesManager()
{
  // Constructor - initialization happens in begin()
}

bool PreferencesManager::begin()
{
  // Open NVS namespace in read-write mode
  if (!prefs.begin(NAMESPACE, false))
  {
    DEBUG_E("PreferencesManager: Failed to begin NVS namespace '%s'", NAMESPACE);
    return false;
  }

  DEBUG_I("PreferencesManager: NVS namespace '%s' opened successfully", NAMESPACE);
  return true;
}

void PreferencesManager::loadSettings(SystemStatus *status)
{
  if (status == nullptr)
  {
    DEBUG_E("PreferencesManager: loadSettings - null status pointer");
    return;
  }

  // Load motorSpeed
  uint8_t motorSpeed = prefs.getUChar(KEY_MOTOR_SPEED, DEFAULT_MOTOR_SPEED);
  if (!validateMotorSpeed(motorSpeed))
  {
    DEBUG_W("PreferencesManager: Invalid motorSpeed loaded (%u), using default", motorSpeed);
    motorSpeed = DEFAULT_MOTOR_SPEED;
  }
  status->msgMaster.motorSpeed = motorSpeed;
  DEBUG_I("PreferencesManager: Loaded motorSpeed = %u", motorSpeed);

  // Load motorTorque
  uint8_t motorTorque = prefs.getUChar(KEY_MOTOR_TORQUE, DEFAULT_MOTOR_TORQUE);
  if (!validateMotorTorque(motorTorque))
  {
    DEBUG_W("PreferencesManager: Invalid motorTorque loaded (%u), using default", motorTorque);
    motorTorque = DEFAULT_MOTOR_TORQUE;
  }
  status->msgMaster.motorTorque = motorTorque;
  DEBUG_I("PreferencesManager: Loaded motorTorque = %u", motorTorque);

  // Load timeout
  uint32_t timeout = prefs.getUInt(KEY_TIMEOUT, DEFAULT_TIMEOUT_MS);
  if (!validateTimeout(timeout))
  {
    DEBUG_W("PreferencesManager: Invalid timeout loaded (%u), using default", timeout);
    timeout = DEFAULT_TIMEOUT_MS;
  }
  status->msgMaster.timeout = timeout;
  DEBUG_I("PreferencesManager: Loaded timeout = %u ms", timeout);

  // Load calibrationOffset
  float calibrationOffset = prefs.getFloat(KEY_CALIBRATION_OFFSET, DEFAULT_CALIBRATION_OFFSET);
  if (!validateCalibrationOffset(calibrationOffset))
  {
    DEBUG_W("PreferencesManager: Invalid calibrationOffset loaded (%.3f), using default", calibrationOffset);
    calibrationOffset = DEFAULT_CALIBRATION_OFFSET;
  }
  status->calibrationOffset = calibrationOffset;
  DEBUG_I("PreferencesManager: Loaded calibrationOffset = %.3f mm", calibrationOffset);
}

void PreferencesManager::saveMotorSpeed(uint8_t value)
{
  if (!validateMotorSpeed(value))
  {
    DEBUG_E("PreferencesManager: Invalid motorSpeed value (%u), not saving", value);
    return;
  }

  prefs.putUChar(KEY_MOTOR_SPEED, value);
  DEBUG_I("PreferencesManager: Saved motorSpeed = %u", value);
}

void PreferencesManager::saveMotorTorque(uint8_t value)
{
  if (!validateMotorTorque(value))
  {
    DEBUG_E("PreferencesManager: Invalid motorTorque value (%u), not saving", value);
    return;
  }

  prefs.putUChar(KEY_MOTOR_TORQUE, value);
  DEBUG_I("PreferencesManager: Saved motorTorque = %u", value);
}

void PreferencesManager::saveTimeout(uint32_t value)
{
  if (!validateTimeout(value))
  {
    DEBUG_E("PreferencesManager: Invalid timeout value (%u), not saving", value);
    return;
  }

  prefs.putUInt(KEY_TIMEOUT, value);
  DEBUG_I("PreferencesManager: Saved timeout = %u ms", value);
}

void PreferencesManager::saveCalibrationOffset(float value)
{
  if (!validateCalibrationOffset(value))
  {
    DEBUG_E("PreferencesManager: Invalid calibrationOffset value (%.3f), not saving", value);
    return;
  }

  prefs.putFloat(KEY_CALIBRATION_OFFSET, value);
  DEBUG_I("PreferencesManager: Saved calibrationOffset = %.3f mm", value);
}

void PreferencesManager::resetToDefaults()
{
  DEBUG_I("PreferencesManager: Resetting all settings to defaults");

  // Clear all settings
  prefs.clear();

  // Save default values
  prefs.putUChar(KEY_MOTOR_SPEED, DEFAULT_MOTOR_SPEED);
  prefs.putUChar(KEY_MOTOR_TORQUE, DEFAULT_MOTOR_TORQUE);
  prefs.putUInt(KEY_TIMEOUT, DEFAULT_TIMEOUT_MS);
  prefs.putFloat(KEY_CALIBRATION_OFFSET, DEFAULT_CALIBRATION_OFFSET);

  DEBUG_I("PreferencesManager: Settings reset to defaults:");
  DEBUG_I("  motorSpeed = %u", DEFAULT_MOTOR_SPEED);
  DEBUG_I("  motorTorque = %u", DEFAULT_MOTOR_TORQUE);
  DEBUG_I("  timeout = %u ms", DEFAULT_TIMEOUT_MS);
  DEBUG_I("  calibrationOffset = %.3f mm", DEFAULT_CALIBRATION_OFFSET);
}

bool PreferencesManager::isSettingsValid()
{
  // Check if all settings are within valid ranges
  uint8_t motorSpeed = prefs.getUChar(KEY_MOTOR_SPEED, DEFAULT_MOTOR_SPEED);
  uint8_t motorTorque = prefs.getUChar(KEY_MOTOR_TORQUE, DEFAULT_MOTOR_TORQUE);
  uint32_t timeout = prefs.getUInt(KEY_TIMEOUT, DEFAULT_TIMEOUT_MS);
  float calibrationOffset = prefs.getFloat(KEY_CALIBRATION_OFFSET, DEFAULT_CALIBRATION_OFFSET);

  return validateMotorSpeed(motorSpeed) &&
         validateMotorTorque(motorTorque) &&
         validateTimeout(timeout) &&
         validateCalibrationOffset(calibrationOffset);
}

bool PreferencesManager::validateMotorSpeed(uint8_t value) const
{
  return value >= MIN_MOTOR_SPEED && value <= MAX_MOTOR_SPEED;
}

bool PreferencesManager::validateMotorTorque(uint8_t value) const
{
  return value >= MIN_MOTOR_TORQUE && value <= MAX_MOTOR_TORQUE;
}

bool PreferencesManager::validateTimeout(uint32_t value) const
{
  return value >= MIN_TIMEOUT_MS && value <= MAX_TIMEOUT_MS;
}

bool PreferencesManager::validateCalibrationOffset(float value) const
{
  return value >= MIN_CALIBRATION_OFFSET && value <= MAX_CALIBRATION_OFFSET;
}
