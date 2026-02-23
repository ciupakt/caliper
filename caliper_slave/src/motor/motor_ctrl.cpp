/**
 * @file motor_ctrl.cpp
 * @brief STSPIN250 DC Motor Controller Implementation for ESP32
 * @author System Generated
 * @date 2026-02-23
 * @version 3.0
 *
 * @details
 * Implementation for STSPIN250 single H-Bridge DC motor driver.
 * Provides full motor control with speed, direction, current limiting,
 * enable/disable, and fault detection.
 */

#include "motor_ctrl.h"

#include <MacroDebugger.h>
#include <error_handler.h>

//==============================================================================
// Public Function Implementations
//==============================================================================

void motorCtrlInit(void)
{
    DEBUG_I("Initializing STSPIN250 Motor Controller...");

    // Configure output pins
    pinMode(MOTOR_PWM_PIN, OUTPUT);
    pinMode(MOTOR_PH_PIN, OUTPUT);
    pinMode(MOTOR_REF_PIN, OUTPUT);
    pinMode(MOTOR_EN_PIN, OUTPUT);

    // Configure fault input with internal pull-up
    pinMode(MOTOR_FAULT_PIN, INPUT_PULLUP);

    // Initial state - disabled, stopped, zero current limit
    digitalWrite(MOTOR_EN_PIN, LOW); // Disabled for safety
    analogWrite(MOTOR_PWM_PIN, 0);
    digitalWrite(MOTOR_PH_PIN, LOW);
    analogWrite(MOTOR_REF_PIN, 0);

    DEBUG_I("STSPIN250 Motor Controller initialized (disabled)");
}

void motorCtrlEnable(bool enabled)
{
    digitalWrite(MOTOR_EN_PIN, enabled ? HIGH : LOW);
    DEBUG_I("Motor %s", enabled ? "enabled" : "disabled");
}

bool motorCtrlCheckFault(void)
{
    bool fault = (digitalRead(MOTOR_FAULT_PIN) == LOW);
    if (fault)
    {
        RECORD_ERROR(ERR_MOTOR_FAULT, "Motor fault detected - overcurrent or thermal shutdown");
    }
    return fault;
}

/**
 * @brief Controls the DC motor with PWM and current limiting
 *
 * @details
 * STSPIN250 Truth Table (EN=1):
 * | PH | PWM | OUT1 | OUT2 | Condition |
 * |----|-----|------|------|-----------|
 * | 0  |0 | GND  | GND  | Slow decay (brake) |
 * | 0  | PWM | GND  | VS   | Reverse - current X1←X2 |
 * | 1  |0 | GND  | GND  | Slow decay (brake) |
 * | 1  | PWM | VS   | GND  | Forward - current X1→X2 |
 *
 * Motor State Mapping:
 * | MotorState | PH | PWM | Description |
 * |------------|----|----|-------------|
 * | MOTOR_STOP | 0  | 0  | Slow decay |
 * | MOTOR_FORWARD | 1  | speed | Forward rotation |
 * | MOTOR_REVERSE | 0  | speed | Reverse rotation |
 * | MOTOR_BRAKE | 0  | 0  | Same as STOP |
 *
 * @param speed Motor speed (0-255)
 * @param torque Motor current limit (0-255, maps to REF pin voltage)
 * @param direction Motor direction (MotorState enum)
 */
void motorCtrlRun(uint8_t speed, uint8_t torque, MotorState direction)
{
    // Clamp speed to valid range
    speed = constrain(speed, 0, PWM_MAX_VALUE);

    // Clamp torque to valid range (controls current limit via REF pin)
    torque = constrain(torque, 0, PWM_MAX_VALUE);

    // Motor state lookup table
    static const struct
    {
        uint8_t ph;
        const char *name;
    } motorTable[] = {
        {0, "Stop"},        // MOTOR_STOP - PH=0, PWM=0
        {1, "Forward"},     // MOTOR_FORWARD - PH=1, PWM=speed
        {0, "Reverse"},     // MOTOR_REVERSE - PH=0, PWM=speed
        {0, "Brake"}        // MOTOR_BRAKE - PH=0, PWM=0 (same as STOP)
    };

    // Validate direction
    if (direction > MOTOR_BRAKE)
    {
        RECORD_ERROR(ERR_MOTOR_INVALID_DIRECTION, "Invalid direction: %u (valid: 0-3)", (unsigned)direction);
        analogWrite(MOTOR_PWM_PIN, 0);
        digitalWrite(MOTOR_PH_PIN, LOW);
        analogWrite(MOTOR_REF_PIN, 0);
        digitalWrite(MOTOR_EN_PIN, LOW); // Disable on error
        return;
    }

    // Check for fault condition
    if (motorCtrlCheckFault())
    {
        DEBUG_W("Motor fault active - command ignored");
        return;
    }

    // Set current limit via REF pin (torque parameter)
    // torque 0-255 maps to V_REF 0-0.43V via PWM + RC filter + voltage divider
    analogWrite(MOTOR_REF_PIN, torque);

    // Set direction via PH pin
    digitalWrite(MOTOR_PH_PIN, motorTable[direction].ph);

    // Set speed via PWM pin
    if (direction == MOTOR_STOP || direction == MOTOR_BRAKE)
    {
        analogWrite(MOTOR_PWM_PIN, 0); // No PWM for stop/brake
    }
    else
    {
        analogWrite(MOTOR_PWM_PIN, speed); // Direct PWM - no inversion!
    }

    // Optimized debug output (only when speed changes significantly or direction changes)
    static uint8_t lastSpeed = PWM_MAX_VALUE;
    static MotorState lastDirection = MOTOR_STOP;

    if (abs(speed - lastSpeed) > MOTOR_SPEED_CHANGE_THRESHOLD || direction != lastDirection)
    {
        DEBUG_I("Motor: %u/%u (%u%%) torque=%u - %s",
                (unsigned)speed, (unsigned)PWM_MAX_VALUE,
                (unsigned)((speed * 100U) / PWM_MAX_VALUE),
                (unsigned)torque,
                motorTable[direction].name);

        lastSpeed = speed;
        lastDirection = direction;
    }
}
