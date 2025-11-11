/**
 * @file    caliper_slave_motor_ctrl.h
 * @brief   MP6550GG-Z DC Motor Controller Header for ESP32
 * @author  Generated from caliper_slave_motor_ctrl.ino
 * @date    2025-11-11
 * @version 2.0
 * 
 * @details
 * Header file for MP6550GG-Z single H-Bridge DC motor driver.
 * Based on MP6550GG-Z datasheet specifications.
 * 
 * This module provides functions to control a DC motor using the MP6550GG-Z
 * H-bridge driver with integrated current monitoring and protection features.
 * 
 * Features:
 * - PWM Input control mode (IN1/IN2)
 * - Current monitoring via VISEN
 * - Current limiting via ISET
 * - Dual Sleep/Wake functionality (nSLEEP_HB, nSLEEP_LDO)
 * - Built-in 3.3V regulator output
 * - Over-current and thermal protection
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
// Pin definitions for MP6550GG-Z (Single H-Bridge Channel)
//==============================================================================

#define MOTOR_IN1_PIN 12       // IN1 input (PWM control input 1)
#define MOTOR_IN2_PIN 13       // IN2 input (PWM control input 2)
#define MOTOR_ISET_PIN 25       // ISET input (Current programming via DAC)
#define MOTOR_VISEN_PIN 4      // VISEN output (Current sense voltage)
#define MOTOR_nSLEEP_HB_PIN 34  // nSLEEP_HB input (H-bridge sleep control)
#define MOTOR_nSLEEP_LDO_PIN 35 // nSLEEP_LDO input (LDO sleep control)

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
  MOTOR_REVERSE = 2       /**< Reverse rotation - OUT2→OUT1 */
} MotorState;

/**
 * @brief Current control mode settings
 * Note: MP6550GG-Z has built-in cycle-by-cycle current regulation
 */
typedef enum {
  CURRENT_AUTO = 0,        /**< Automatic current regulation (built-in) */
  CURRENT_MANUAL = 1       /**< Manual current limit via ISET */
} CurrentMode;

//==============================================================================
// Data Structures
//==============================================================================

/**
 * @brief Motor control configuration parameters
 */
typedef struct {
  float maxCurrent;           /**< Maximum motor current in Amperes */
  float isetResistance;       /**< ISET resistance value in Ohms */
  CurrentMode currentMode;    /**< Current control mode */
  bool ldoEnabled;           /**< 3.3V LDO regulator state */
} MotorConfig;

//==============================================================================
// Function Declarations
//==============================================================================

/**
 * @brief Initialize the motor controller with default settings
 * @details
 * Configures all pins and sets up MP6550GG-Z for PWM input control mode.
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
 * Sets IN1=HIGH, IN2=LOW
 */
void motorForward(void);

/**
 * @brief Set motor to reverse rotation
 * @details
 * Configures motor for reverse direction (OUT2→OUT1).
 * Sets IN1=LOW, IN2=HIGH
 */
void motorReverse(void);

/**
 * @brief Stop motor (coast mode)
 * @details
 * Puts motor in coast mode where both outputs are Hi-Z.
 * allowing the motor to coast to a stop naturally.
 * Sets IN1=LOW, IN2=LOW
 */
void motorStop(void);

/**
 * @brief Put motor in sleep mode
 * @details
 * Enters low power consumption mode by setting both nSLEEP_HB and nSLEEP_LDO low.
 * All H-bridge outputs and 3.3V regulator are disabled during sleep.
 */
void motorSleep(void);

/**
 * @brief Wake motor from sleep mode
 * @details
 * Exits sleep mode and enables normal operation.
 * Waits for tWAKE requirement (200μs max) before operation.
 */
void motorWake(void);

/**
 * @brief Set the motor current limit
 * @param currentAmps Maximum current in Amperes (0.1 to 2.0A)
 * @details
 * Sets the current limit using ISET pin. Current limit is calculated as:
 * I_LIMIT = 0.5V / R_ISET
 * 
 * For 1A current: R_ISET = 0.5kΩ
 * For 2A current: R_ISET = 0.25kΩ
 * 
 * Uses DAC on GPIO25 to simulate variable resistance.
 * 
 * @note Current is automatically clamped to 2.0A for safety
 */
void setCurrentLimit(float currentAmps);

/**
 * @brief Read the motor current from VISEN pin
 * @return Current in Amperes
 * @details
 * Reads the current using VISEN pin which outputs a voltage
 * proportional to the motor current.
 * VISEN = I_OUT * R_ISET * (100μA/A)
 */
float readMotorCurrent(void);

/**
 * @brief Check for motor faults
 * @return true if fault detected, false otherwise
 * @details
 * MP6550GG-Z doesn't have dedicated nFAULT pin.
 * Fault detection is based on:
 * - Over-current detection (current > limit)
 * - Thermal shutdown (current suddenly drops to 0)
 * - Under-voltage detection (VCC monitoring)
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
 * - LDO regulator state
 */
void getMotorStatus(char* buffer, int bufferSize);

/**
 * @brief Configure the current control mode
 * @param mode Current control mode to set
 * @details
 * Configures current control behavior:
 * - CURRENT_AUTO: Built-in cycle-by-cycle regulation
 * - CURRENT_MANUAL: Manual current limit via ISET
 */
void configureCurrentMode(CurrentMode mode);

/**
 * @brief Set complete motor configuration
 * @param config Motor configuration structure
 * @details
 * Sets all motor configuration parameters including current limit,
 * current control mode, and LDO state.
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

/**
 * @brief Enable/disable 3.3V LDO regulator
 * @param enabled true to enable LDO, false to disable
 * @details
 * Controls the nSLEEP_LDO pin to enable/disable the 3.3V regulator.
 * Useful for power saving in battery applications.
 */
void setLDOState(bool enabled);

/**
 * @brief Get 3.3V LDO regulator state
 * @return true if LDO is enabled, false otherwise
 */
bool getLDOState(void);

#ifdef __cplusplus
}
#endif

#endif /* CALIPER_SLAVE_MOTOR_CTRL_H */