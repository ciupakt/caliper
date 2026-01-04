/**
 * @file motor_ctrl.h
 * @brief MP6550GG-Z DC Motor Controller Header for ESP32
 * @author System Generated
 * @date 2025-12-27
 * @version 1.0
 *
 * @details
 * Minimalist header file for MP6550GG-Z single H-Bridge DC motor driver.
 * Provides basic motor control functionality with PWM.
 *
 * Features:
 * - PWM Input control mode (IN1/IN2)
 * - Simple forward/reverse/stop/brake control
 */

#ifndef MOTOR_CTRL_H
#define MOTOR_CTRL_H

#include <Arduino.h>
#include <shared_common.h>
#include <shared_config.h>

// Motor pin definitions
#define MOTOR_IN2_PIN 12
#define MOTOR_IN1_PIN 13

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize the motor controller
     * @details
     * Configures motor pins for PWM control.
     * This function must be called before using any other motor control functions.
     */
    void motorCtrlInit(void);

    /**
     * @brief Set motor speed and direction
     * @param speed Motor speed (0-255)
     * @param direction Motor direction (MOTOR_STOP, MOTOR_FORWARD, MOTOR_REVERSE, MOTOR_BRAKE)
     * @details
     * Sets PWM-controlled motor speed. Speed 0 stops the motor, 255 is maximum speed.
     */
    void motorCtrlRun(uint8_t speed, uint8_t torque, MotorState direction);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_CTRL_H */
