/**
 * @file motor_ctrl.cpp
 * @brief MP6550GG-Z DC Motor Controller Implementation for ESP32
 * @author System Generated
 * @date 2025-12-27
 * @version 2.0
 *
 * @details
 * Minimalist implementation for MP6550GG-Z single H-Bridge DC motor driver.
 * Provides basic motor control functionality with PWM.
 *
 * @version 2.0 - Integrated comprehensive error code system
 */

#include "motor_ctrl.h"

#include <MacroDebugger.h>
#include <error_handler.h>

//==============================================================================
// Public Function Implementations
//==============================================================================

void motorCtrlInit(void)
{
    DEBUG_I("Initializing MP6550GG-Z Motor Controller...");

    // Configure pins as outputs/inputs
    pinMode(MOTOR_IN1_PIN, OUTPUT);
    pinMode(MOTOR_IN2_PIN, OUTPUT);
}

/**
 * @brief Steruje silnikiem DC z użyciem PWM
 *
 * Funkcja kontroluje silnik DC MP6550GG-Z poprzez sterowanie pinami
 * IN1 i IN2 w trybie PWM Input Control.
 *
 * @details
 * Parametry:
 * - speed: prędkość silnika (0-255, gdzie 255 to maksymalna prędkość)
 * - torque: moment obrotowy (obecnie nieużywany, MP6550GG-Z nie wspiera kontroli momentu)
 * - direction: kierunek obrotów (MOTOR_STOP, MOTOR_FORWARD, MOTOR_REVERSE, MOTOR_BRAKE)
 *
 * Tabela motorTable - mapowanie MotorState na stany pinów:
 *
 * | MotorState      | IN1  | IN2  | Opis                    |
 * |-----------------|-------|-------|--------------------------|
 * | MOTOR_STOP       | 0     | 0     | Silnik zatrzymany (coast) |
 * | MOTOR_FORWARD    | PWM   | 255   | Silnik do przodu          |
 * | MOTOR_REVERSE    | 255   | PWM   | Silnik do tyłu            |
 * | MOTOR_BRAKE      | 255   | 255   | Hamowanie (brake)          |
 *
 * Logika sterowania PWM:
 * - MOTOR_FORWARD: IN1 = 255 - speed, IN2 = 255
 *   - Im mniejsze speed, tym większe napięcie na IN1 (szybciej)
 *   - IN2 jest zawsze na 255 (GND)
 *   - Inwersja PWM jest wymagana przez specyfikację MP6550GG-Z
 *
 * - MOTOR_REVERSE: IN1 = 255, IN2 = 255 - speed
 *   - Analogicznie do FORWARD, ale odwrócone piny
 *
 * - MOTOR_STOP: IN1 = 0, IN2 = 0
 *   - Oba piny w stanie LOW (silnik swobodnie się kręci)
 *
 * - MOTOR_BRAKE: IN1 = 255, IN2 = 255
 *   - Oba piny w stanie HIGH (silnik hamuje)
 *
 * Optymalizacja logowania:
 * - Funkcja loguje tylko gdy:
 *   1. Zmiana prędkości > MOTOR_SPEED_CHANGE_THRESHOLD (10)
 *   2. Zmiana kierunku
 * - Zapobiega spamowaniu logów przy drobnych zmianach prędkości
 *
 * Walidacja:
 * - speed jest ograniczony do zakresu 0-255 (constrain)
 * - torque jest ograniczony do zakresu 0-255 (ale nieużywany)
 * - direction jest walidowany - jeśli > MOTOR_BRAKE, silnik jest zatrzymywany
 *
 * Uwaga: Parametr torque jest obecnie nieużywany, ponieważ MP6550GG-Z
 * nie wspiera bezpośredniej kontroli momentu obrotowego. Jest zachowany
 * dla przyszłej kompatybilności.
 *
 * @param speed Prędkość silnika (0-255)
 * @param torque Moment obrotowy (0-255, obecnie nieużywany)
 * @param direction Kierunek obrotów (MotorState)
 */
void motorCtrlRun(uint8_t speed, uint8_t torque, MotorState direction)
{
    // Clamp speed to valid range
    speed = constrain(speed, 0, PWM_MAX_VALUE);

    // TODO: Currently torque parameter is unused, as MP6550GG-Z does not support torque control directly
    torque = constrain(torque, 0, PWM_MAX_VALUE);

    // Optimized lookup table for motor control
    static const struct
    {
        uint8_t in1, in2;
        const char *name;
    } motorTable[] = {
        {0, 0, "Stop"},      // MOTOR_STOP
        {0, PWM_MAX_VALUE, "Forward"}, // MOTOR_FORWARD (will be modified below)
        {PWM_MAX_VALUE, 0, "Reverse"}, // MOTOR_REVERSE (will be modified below)
        {PWM_MAX_VALUE, PWM_MAX_VALUE, "Brake"}  // MOTOR_BRAKE
    };

    if (direction > MOTOR_BRAKE)
    {
        RECORD_ERROR(ERR_MOTOR_INVALID_DIRECTION, "Invalid direction: %u (valid: 0-3)", (unsigned)direction);
        digitalWrite(MOTOR_IN1_PIN, LOW);
        digitalWrite(MOTOR_IN2_PIN, LOW);
        return;
    }

    // Handle PWM cases with speed control
    if (direction == MOTOR_FORWARD)
    {
        analogWrite(MOTOR_IN1_PIN, PWM_MAX_VALUE - speed);
        analogWrite(MOTOR_IN2_PIN, PWM_MAX_VALUE);
    }
    else if (direction == MOTOR_REVERSE)
    {
        analogWrite(MOTOR_IN1_PIN, PWM_MAX_VALUE);
        analogWrite(MOTOR_IN2_PIN, PWM_MAX_VALUE - speed);
    }
    else
    {
        // Direct values for STOP and BRAKE
        analogWrite(MOTOR_IN1_PIN, motorTable[direction].in1);
        analogWrite(MOTOR_IN2_PIN, motorTable[direction].in2);
    }

    // Optimized debug output (only when speed changes significantly or direction changes)
    static uint8_t lastSpeed = PWM_MAX_VALUE;
    static MotorState lastDirection = MOTOR_STOP;

    if (abs(speed - lastSpeed) > MOTOR_SPEED_CHANGE_THRESHOLD || direction != lastDirection)
    {
        DEBUG_I("Motor: %u/%u (%u%%) - %s", (unsigned)speed, (unsigned)PWM_MAX_VALUE, (unsigned)((speed * 100U) / PWM_MAX_VALUE), motorTable[direction].name);

        lastSpeed = speed;
        lastDirection = direction;
    }
}
