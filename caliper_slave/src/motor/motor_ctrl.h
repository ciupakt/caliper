/**
 * @file motor_ctrl.h
 * @brief STSPIN250 DC Motor Controller Header for ESP32
 * @author System Generated
 * @date 2026-02-23
 * @version 3.0
 *
 * @details
 * Implementation for STSPIN250 single H-Bridge DC motor driver with full control.
 *
 * Features:
 * - PWM speed control via PWM pin
 * - Direction control via PH pin
 * - Software-adjustable current limiting via REF pin (PWM + RC filter)
 * - Enable/disable control via EN pin
 * - Fault detection via FAULT pin (overcurrent, thermal shutdown)
 * - STBY/RESET hardwired to VDD (no standby mode)
 *
 * Pin Mapping:
 * | GPIO | STSPIN250 Pin | Function |
 * |------|---------------|----------|
 * | 6    | PWM           | Speed control |
 * | 15   | PH            | Direction (0=reverse, 1=forward) |
 * | 7    | REF           | Current limit via PWM + RC filter |
 * | 16   | EN            | Enable (HIGH=enabled) |
 * | 17   | FAULT         | Fault detection (LOW=fault) |
 *
 * Current Limiting:
 * - torque parameter (0-255) controls current limit via REF pin
 * - PWM → RC filter → voltage divider → REF pin (0-0.5V)
 * - I_peak = V_REF / R_SNS (typical R_SNS = 0.33Ω)
 */

#ifndef MOTOR_CTRL_H
#define MOTOR_CTRL_H

#include <Arduino.h>
#include <shared_common.h>
#include <shared_config.h>
#include <error_handler.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize the motor controller
     * @details
     * Configures all motor pins (PWM, PH, REF, EN, FAULT).
     * Motor starts in disabled state with zero current limit.
     * This function must be called before using any other motor control functions.
     */
    void motorCtrlInit(void);

    /**
     * @brief Enable or disable the motor driver
     * @param enabled true to enable, false to disable
     * @details
     * When disabled, the H-bridge outputs are in high-impedance state.
     * Should be called before motorCtrlRun() to enable motor operation.
     */
    void motorCtrlEnable(bool enabled);

    /**
     * @brief Check for fault condition
     * @return true if fault detected (overcurrent or thermal shutdown)
     * @details
     * The FAULT pin goes LOW when:
     * - Overcurrent condition detected
     * - Thermal shutdown triggered
     * - Short circuit detected
     *
     * When a fault is detected, the motor is automatically disabled
     * by the STSPIN250 hardware.
     */
    bool motorCtrlCheckFault(void);

    /**
     * @brief Set motor speed, current limit, and direction
     * @param speed Motor speed (0-255)
     * @param torque Motor current limit (0-255, maps to 0-0.43V on REF pin)
     * @param direction Motor direction (MOTOR_STOP, MOTOR_FORWARD, MOTOR_REVERSE, MOTOR_BRAKE)
     * @details
     * Sets PWM-controlled motor speed with current limiting.
     *
     * Speed control:
     * - 0 = stopped
     * - 255 = maximum speed
     *
     * Current limiting (torque):
     * - 0 = no current (motor won't run)
     * - 255 = maximum current (~1.3A with typical circuit)
     *
     * Direction mapping:
     * - MOTOR_STOP: PH=0, PWM=0 (slow decay)
     * - MOTOR_FORWARD: PH=1, PWM=speed
     * - MOTOR_REVERSE: PH=0, PWM=speed
     * - MOTOR_BRAKE: PH=0, PWM=0 (slow decay, same as STOP)
     *
     * Possible errors:
     * - ERR_MOTOR_INVALID_DIRECTION: Invalid direction specified
     * - ERR_MOTOR_FAULT: Fault condition detected
     */
    void motorCtrlRun(uint8_t speed, uint8_t torque, MotorState direction);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_CTRL_H */
