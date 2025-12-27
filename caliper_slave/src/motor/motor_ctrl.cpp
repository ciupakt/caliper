/**
 * @file motor_ctrl.cpp
 * @brief MP6550GG-Z DC Motor Controller Implementation for ESP32
 * @author System Generated
 * @date 2025-12-27
 * @version 1.0
 *
 * @details
 * Minimalist implementation for MP6550GG-Z single H-Bridge DC motor driver.
 * Provides basic motor control functionality with PWM.
 */

#include "motor_ctrl.h"

#include <MacroDebugger.h>

//==============================================================================
// Public Function Implementations
//==============================================================================

void initializeMotorController(void)
{
    DEBUG_I("Initializing MP6550GG-Z Motor Controller...");

    // Configure pins as outputs/inputs
    pinMode(MOTOR_IN1_PIN, OUTPUT);
    pinMode(MOTOR_IN2_PIN, OUTPUT);
}

void setMotorSpeed(uint8_t speed, MotorState direction)
{
    // Clamp speed to valid range
    speed = constrain(speed, 0, 255);

    // Optimized lookup table for motor control
    static const struct
    {
        uint8_t in1, in2;
        const char *name;
    } motorTable[] = {
        {0, 0, "Stop"},      // MOTOR_STOP
        {0, 255, "Forward"}, // MOTOR_FORWARD (will be modified below)
        {255, 0, "Reverse"}, // MOTOR_REVERSE (will be modified below)
        {255, 255, "Brake"}  // MOTOR_BRAKE
    };

    if (direction > MOTOR_BRAKE)
    {
        DEBUG_E("Error: Invalid motor direction");
        digitalWrite(MOTOR_IN1_PIN, LOW);
        digitalWrite(MOTOR_IN2_PIN, LOW);
        return;
    }

    // Handle PWM cases with speed control
    if (direction == MOTOR_FORWARD)
    {
        analogWrite(MOTOR_IN1_PIN, 255 - speed);
        analogWrite(MOTOR_IN2_PIN, 255);
    }
    else if (direction == MOTOR_REVERSE)
    {
        analogWrite(MOTOR_IN1_PIN, 255);
        analogWrite(MOTOR_IN2_PIN, 255 - speed);
    }
    else
    {
        // Direct values for STOP and BRAKE
        analogWrite(MOTOR_IN1_PIN, motorTable[direction].in1);
        analogWrite(MOTOR_IN2_PIN, motorTable[direction].in2);
    }

    // Optimized debug output (only when speed changes significantly or direction changes)
    static uint8_t lastSpeed = 255;
    static MotorState lastDirection = MOTOR_STOP;

    if (abs(speed - lastSpeed) > 10 || direction != lastDirection)
    {
        DEBUG_I("Motor: %u/255 (%u%%) - %s", (unsigned)speed, (unsigned)((speed * 100U) / 255U), motorTable[direction].name);

        lastSpeed = speed;
        lastDirection = direction;
    }
}
