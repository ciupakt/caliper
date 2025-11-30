/** @file    caliper_slave_motor_ctrl.h
 * @brief   Minimalist MP6550GG-Z DC Motor Controller Header for ESP32
 * @author  Generated from caliper_slave_motor_ctrl.ino
 * @date    2025-11-11
 * @version 2.0
 *
 * @details
 * Minimalist header file for MP6550GG-Z single H-Bridge DC motor driver.
 * Provides basic motor control functionality with PWM.
 *
 * Features:
 * - PWM Input control mode (IN1/IN2)
 * - Simple forward/reverse/stop/brake control
 */

#ifndef CALIPER_SLAVE_MOTOR_CTRL_H
#define CALIPER_SLAVE_MOTOR_CTRL_H

#include <Arduino.h>

// Motor pin definitions
#define MOTOR_IN2_PIN   12
#define MOTOR_IN1_PIN   13

// Motor state enumeration
typedef enum {
  MOTOR_STOP = 0,     /**< Motor stopped (coast) */
  MOTOR_FORWARD = 1,  /**< Motor rotating forward */
  MOTOR_REVERSE = 2,  /**< Motor rotating reverse */
  MOTOR_BRAKE = 3     /**< Motor braking */
} MotorState;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the motor controller
 * @details
 * Configures motor pins for PWM control.
 * This function must be called before using any other motor control functions.
 */
void initializeMotorController(void);

/**
 * @brief Set motor speed and direction
 * @param speed Motor speed (0-255)
 * @param direction Motor direction (MOTOR_STOP, MOTOR_FORWARD, MOTOR_REVERSE, MOTOR_BRAKE)
 * @details
 * Sets PWM-controlled motor speed. Speed 0 stops the motor, 255 is maximum speed.
 */
void setMotorSpeed(uint8_t speed, MotorState direction);

#ifdef __cplusplus
}
#endif

#endif /* CALIPER_SLAVE_MOTOR_CTRL_H */