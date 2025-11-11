/**
 * @file    caliper_slave_motor_ctrl.h
 * @brief   TB67H453FNG DC Motor Controller Header for ESP32
 * @author  Generated from caliper_slave_motor_ctrl.ino
 * @date    2025-11-10
 * @version 1.0
 * 
 * @details
 * Header file for TB67H453FNG single H-Bridge DC motor driver.
 * Based on TB67H453FNG datasheet specifications.
 * 
 * This module provides functions to control a DC motor using the TB67H453FNG
 * H-bridge driver with integrated current monitoring and protection features.
 * 
 * Features:
 * - Phase/Enable control mode
 * - Current monitoring via ISENSE
 * - Fault detection via nFAULT
 * - Current limiting via VREF
 * - Sleep/Wake functionality
 * - Current control modes
 */

#ifndef CALIPER_SLAVE_MOTOR_CTRL_H
#define CALIPER_SLAVE_MOTOR_CTRL_H

#include <Arduino.h>

// Arduino serial object is automatically available
// No need for special declarations

#ifdef __cplusplus
extern "C" {
#endif

//==============================================================================
// Pin definitions for TB67H453FNG (Single H-Bridge Channel)
//==============================================================================

#define MOTOR_EN_IN1_PIN 12    // EN/IN1 input (Enable/IN1 control)
#define MOTOR_PH_IN2_PIN 13   // PH/IN2 input (Phase/IN2 control)
#define MOTOR_VREF_PIN 25     // VREF input (Current limit reference)
#define MOTOR_ISENSE_PIN 4    // ISENSE output (Current monitoring)
#define MOTOR_nFAULT_PIN 15    // nFAULT output (Error detection)
#define MOTOR_PMODE_PIN 36    // PMODE input (Control mode setting)
#define MOTOR_IMODE_PIN 39    // IMODE input (Current control mode setting)
#define MOTOR_nSLEEP_PIN 34   // nSLEEP input (Sleep/Normal operation)

//==============================================================================
// Enumerations
//==============================================================================

/**
 * @brief Motor operational states
 */
typedef enum {
  MOTOR_SLEEP = -1,      /**< Sleep mode - low power consumption */
  MOTOR_STOP = 0,        /**< Stop/Coast mode - Hi-Z output */
  MOTOR_FORWARD = 1,     /**< Forward rotation - OUT1→OUT2 */
  MOTOR_REVERSE = 2      /**< Reverse rotation - OUT2→OUT1 */
} MotorState;

/**
 * @brief Current control mode settings
 */
typedef enum {
  CURRENT_DISABLED = 0,      /**< Current control disabled */
  CURRENT_CONSTANT = 1,      /**< Constant current PWM mode */
  CURRENT_FIXED_OFF = 2      /**< Fixed off time control mode */
} CurrentMode;

//==============================================================================
// Data Structures
//==============================================================================

/**
 * @brief Motor control configuration parameters
 */
typedef struct {
  float maxCurrent;           /**< Maximum motor current in Amperes */
  float vrefVoltage;          /**< VREF voltage for current limit */
  CurrentMode currentMode;    /**< Current control mode */
} MotorConfig;

//==============================================================================
// Function Declarations
//==============================================================================

/**
 * @brief Initialize the motor controller with default settings
 * @details
 * Configures all pins and sets up TB67H453FNG for Phase/Enable control mode.
 * This function must be called before using any other motor control functions.
 * 
 * @note This function is blocking and takes approximately 100ms to complete
 */
void initializeMotorController(void);

/**
 * @brief Set the motor operational state
 * @param state Motor state to set
 * @return true if state change successful, false if fault detected
 */
bool setMotorState(MotorState state);

/**
 * @brief Set motor to forward rotation
 * @details
 * Configures motor for forward direction (OUT1→OUT2).
 * Motor must be in forward state to rotate forward.
 */
void motorForward(void);

/**
 * @brief Set motor to reverse rotation
 * @details
 * Configures motor for reverse direction (OUT2→OUT1).
 * Motor must be in reverse state to rotate reverse.
 */
void motorReverse(void);

/**
 * @brief Stop motor (coast mode)
 * @details
 * Puts motor in coast mode where both outputs are Hi-Z,
 * allowing the motor to coast to a stop naturally.
 */
void motorStop(void);

/**
 * @brief Put motor in sleep mode
 * @details
 * Enters low power consumption mode by setting nSLEEP low.
 * All H-bridge outputs are disabled during sleep.
 */
void motorSleep(void);

/**
 * @brief Wake motor from sleep mode
 * @details
 * Exits sleep mode and enables normal operation.
 * Waits for tWAKE requirement (1.5ms max) before operation.
 */
void motorWake(void);

/**
 * @brief Set the motor current limit
 * @param currentAmps Maximum current in Amperes (0.1 to 2.45A)
 * @details
 * Sets the current limit using VREF pin. Current limit is calculated as:
 * VREF = I_LIMIT * R_ISENSE * 1/A(ISENSE)
 * 
 * With R_ISENSE = 1.5kΩ, A(ISENSE) = 1000μA/A
 * VREF = I_LIMIT * 1.5kΩ * 0.001 = I_LIMIT * 1.5V
 * 
 * @note Current is automatically clamped to 2.45A for thermal safety
 */
void setCurrentLimit(float currentAmps);

/**
 * @brief Read the motor current from ISENSE pin
 * @return Current in Amperes
 * @details
 * Reads the current using the ISENSE pin which outputs a current
 * proportional to the motor current scaled by A(ISENSE) = 1000μA/A
 */
float readMotorCurrent(void);

/**
 * @brief Check for motor faults
 * @return true if fault detected, false otherwise
 * @details
 * Reads the nFAULT pin which indicates:
 * - Overcurrent (ISD)
 * - Over-temperature (TSD)
 * - Under-voltage lockout (UVLO)
 * - Current limit exceeded (INF)
 */
bool checkMotorFault(void);

/**
 * @brief Get current motor status as string buffer
 * @param buffer Character buffer to store status
 * @param bufferSize Size of the buffer
 * @details
 * Fills the buffer with formatted status containing:
 * - Current motor state
 * - Actual motor current
 * - Fault status
 * - Current limit setting
 */
void getMotorStatus(char* buffer, int bufferSize);

/**
 * @brief Configure the current control mode
 * @param mode Current control mode to set
 * @details
 * Configures the IMODE pin for different current control behaviors:
 * - CURRENT_DISABLED: No current limiting
 * - CURRENT_CONSTANT: Constant current PWM
 * - CURRENT_FIXED_OFF: Fixed off time control
 * 
 * @note This function automatically handles sleep/wake requirements
 */
void configureCurrentMode(CurrentMode mode);

/**
 * @brief Set complete motor configuration
 * @param config Motor configuration structure
 * @details
 * Sets all motor configuration parameters including current limit
 * and current control mode.
 */
void setMotorConfig(MotorConfig config);

/**
 * @brief Get current motor state
 * @return Current MotorState
 */
MotorState getMotorState(void);

/**
 * @brief Check if motor controller is initialized
 * @return true if initialized, false otherwise
 */
bool isMotorInitialized(void);

/**
 * @brief Demo function to test motor operations
 * @details
 * Executes a test sequence demonstrating all motor functions:
 * 1. Forward rotation
 * 2. Stop
 * 3. Reverse rotation
 * 4. Final stop
 * 
 * Each step is logged and current is monitored.
 */
void demoMotorControl(void);

//==============================================================================
// Additional Utility Functions
//==============================================================================

/**
 * @brief Reset motor controller to initial state
 * @details
 * Resets all settings and re-initializes the motor controller.
 * This is equivalent to calling initializeMotorController() again.
 */
void resetMotorController(void);

/**
 * @brief Emergency stop - immediately halt motor
 * @details
 * Immediately stops the motor by setting sleep mode.
 * Use this function in fault conditions or emergency situations.
 */
void emergencyStop(void);

#ifdef __cplusplus
}
#endif

#endif /* CALIPER_SLAVE_MOTOR_CTRL_H */