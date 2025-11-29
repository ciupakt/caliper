/** @file    caliper_slave_motor_ctrl.h
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
 * - Over-current and thermal protection
 * 
 * Note: Sleep/wake functionality and voltage monitoring features are not available
 * in current hardware implementation as related pins are not connected.
 */

#ifndef CALIPER_SLAVE_MOTOR_CTRL_H
#define CALIPER_SLAVE_MOTOR_CTRL_H

#include <Arduino.h>

// Arduino serial object is automatically available
// No need for special declarations

// Motor pin definitions
#define MOTOR_IN1_PIN   13
#define MOTOR_IN2_PIN   12
#define MOTOR_ISET_PIN  25
#define MOTOR_VISEN_PIN 26


// PWM configuration
#define MOTOR_PWM_FREQ      5000  // PWM frequency in Hz (up to 100kHz per datasheet)
#define MOTOR_PWM_RESOLUTION 8    // PWM resolution (0-255)

// Motor state enumeration
typedef enum {
  MOTOR_SLEEP = 0,    /**< Motor in sleep mode */
  MOTOR_STOP = 1,     /**< Motor stopped (coast) */
  MOTOR_FORWARD = 2,  /**< Motor rotating forward */
  MOTOR_REVERSE = 3,  /**< Motor rotating reverse */
  MOTOR_BRAKE = 4     /**< Motor braking */
} MotorState;

#ifdef __cplusplus
extern "C" {
#endif


/**
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
  uint8_t motorSpeed;       /**< Motor speed (0-255) */
  bool pwmEnabled;         /**< PWM speed control enabled */
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
 * @brief Brake motor
 * @details
 * Configures motor for braking (OUT1=L, OUT2=L or OUT1=H, OUT2=H).
 * MP6550GG-Z: IN1=HIGH, IN2=HIGH for Brake
 */
void motorBrake(void);

/**
 * @brief Stop motor (coast mode)
 * @details
 * Puts motor in coast mode where both outputs are Hi-Z.
 * allowing the motor to coast to a stop naturally.
 * Sets IN1=LOW, IN2=LOW
 */
void motorStop(void);

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
 */
bool checkMotorFault(void);

/**
 * @brief Get current motor status as string buffer
 * @param buffer Character buffer to store status
 * @param bufferSize Size of buffer
 * @details
 * Fills buffer with formatted status containing:
 * - Current motor state
 * - Actual motor current
 * - Fault status
 * - Current limit setting
 */
void getMotorStatus(char* buffer, int bufferSize);

/**
 * @brief Set complete motor configuration
 * @param config Motor configuration structure
 * @details
 * Sets all motor configuration parameters including current limit
 * and current control mode.
 */
void setMotorConfig(MotorConfig config);

/**
 * @brief Test MP6550GG-Z compliance and functionality
 * @details
 * Comprehensive test function to verify all MP6550GG-Z features:
 * 1. Pin configuration verification
 * 2. Current sensing accuracy
 * 3. PWM speed control
 * 4. Fault detection systems
 * 5. Complete status reporting
 * 
 * Use this function to validate hardware connections and software compliance.
 */
void testMotorController(void);

/**
 * @brief Emergency stop - immediately halt motor
 * @details
 * Immediately stops the motor.
 * Use this function in fault conditions or emergency situations.
 */
void emergencyStop(void);

/**
 * @brief Set motor speed with PWM control
 * @param speed Motor speed (0-255)
 * @param direction Motor direction (forward/reverse)
 * @details
 * Sets PWM-controlled motor speed using ESP32 LEDC.
 * Speed 0 stops the motor, 255 is maximum speed.
 */
void setMotorSpeed(uint8_t speed, MotorState direction);

/**
 * @brief Get current motor speed setting
 * @return Current motor speed (0-255)
 */
uint8_t getMotorSpeed(void);

/**
 * @brief Run motor control demonstration
 * @details
 * Executes a complete motor control demonstration sequence
 * including forward, reverse, stop operations with current monitoring.
 */
void demoMotorControl(void);

/**
 * @brief Configure current regulation mode
 * @param mode Current mode to set (AUTO or MANUAL)
 */
void configureCurrentMode(CurrentMode mode);

/**
 * @brief Reset motor controller to initial state
 * @details
 * Resets motor controller and reinitializes with default settings.
 */
void resetMotorController(void);

/**
 * @brief Get current motor state
 * @return Current motor state
 */
MotorState getMotorState(void);

/**
 * @brief Check if motor controller is initialized
 * @return true if initialized, false otherwise
 */
bool isMotorInitialized(void);



#ifdef __cplusplus
}
#endif

#endif /* CALIPER_SLAVE_MOTOR_CTRL_H */