/**
 * @file    caliper_slave_motor_ctrl.cpp
 * @brief   Minimalist MP6550GG-Z DC Motor Controller Implementation for ESP32
 * @author  Generated from caliper_slave_motor_ctrl.ino
 * @date    2025-11-11
 * @version 2.0
 *
 * @details
 * Minimalist implementation for MP6550GG-Z single H-Bridge DC motor driver.
 * Provides basic motor control functionality with PWM.
 */

#include "caliper_slave_motor_ctrl.h"

//==============================================================================
// Public Function Implementations
//==============================================================================

void initializeMotorController(void)
{

  Serial.println("Initializing MP6550GG-Z Motor Controller...");

  // Configure pins as outputs/inputs
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);

}

void setMotorSpeed(uint8_t speed, MotorState direction)
{
  // Clamp speed to valid range
  speed = constrain(speed, 0, 255);

  switch (direction)
  {
  case MOTOR_FORWARD:
    // Forward: IN1=PWM, IN2=HIGH
    analogWrite(MOTOR_IN1_PIN, 255 - speed);
    analogWrite(MOTOR_IN2_PIN, 255);
    break;

  case MOTOR_REVERSE:
    // Reverse: IN1=HIGH, IN2=PWM
    analogWrite(MOTOR_IN1_PIN, 255);
    analogWrite(MOTOR_IN2_PIN, 255 - speed);
    break;

  case MOTOR_BRAKE:
    // Brake: IN1=HIGH, IN2=HIGH
    analogWrite(MOTOR_IN1_PIN, 255);
    analogWrite(MOTOR_IN2_PIN, 255);
    break;

  case MOTOR_STOP:
    // Stop: IN1=LOW, IN2=LOW
    analogWrite(MOTOR_IN1_PIN, 0);
    analogWrite(MOTOR_IN2_PIN, 0);
    break;

  default:
    Serial.println("Error: Invalid motor direction");
    digitalWrite(MOTOR_IN1_PIN, LOW);
    digitalWrite(MOTOR_IN2_PIN, LOW);
    return;
  }

  Serial.print("Motor: ");
  Serial.print(speed);
  Serial.print("/255 (");
  Serial.print((speed * 100) / 255);
  Serial.print("%) - ");
  Serial.println(direction == MOTOR_FORWARD ? "Forward" :
               direction == MOTOR_REVERSE ? "Reverse" :
               direction == MOTOR_BRAKE ? "Brake" : "Stop");
}
