/**
 * @file    caliper_slave_motor_ctrl.cpp
 * @brief   TB67H453FNG DC Motor Controller Implementation for ESP32
 * @author  Generated from caliper_slave_motor_ctrl.ino
 * @date    2025-11-10
 * @version 1.0
 * 
 * @details
 * Implementation file for TB67H453FNG single H-Bridge DC motor driver.
 * Provides complete motor control functionality with current monitoring
 * and fault protection.
 */

#include "caliper_slave_motor_ctrl.h"

//==============================================================================
// Global Variables
//==============================================================================

/** Current motor state */
static MotorState currentMotorState = MOTOR_SLEEP;

/** Current motor configuration */
static MotorConfig currentConfig;

/** Motor controller initialization flag */
static bool motorInitialized = false;

/** Flag to prevent recursive initialization */
static bool initInProgress = false;

//==============================================================================
// Private Function Prototypes
//==============================================================================

/**
 * @brief Internal function to set pin levels safely
 * @param pin Pin number
 * @param value Pin value (HIGH/LOW)
 */
static void safeDigitalWrite(int pin, int value);

/**
 * @brief Internal function to delay with fault checking
 * @param ms Milliseconds to delay
 */
static void safeDelay(unsigned long ms);

/**
 * @brief Internal function to handle mode change requirements
 * @param targetMode Target current control mode
 */
static void changeCurrentMode(CurrentMode targetMode);

//==============================================================================
// Public Function Implementations
//==============================================================================

void initializeMotorController(void) {
  // Prevent recursive initialization
  if (initInProgress || motorInitialized) {
    return;
  }
  
  initInProgress = true;
  Serial.println("Initializing TB67H453FNG Motor Controller...");
  
  // Configure pins as outputs/inputs
  pinMode(MOTOR_EN_IN1_PIN, OUTPUT);
  pinMode(MOTOR_PH_IN2_PIN, OUTPUT);
  pinMode(MOTOR_VREF_PIN, OUTPUT);
  pinMode(MOTOR_PMODE_PIN, OUTPUT);
  pinMode(MOTOR_IMODE_PIN, OUTPUT);
  pinMode(MOTOR_nSLEEP_PIN, OUTPUT);
  pinMode(MOTOR_nFAULT_PIN, INPUT_PULLUP);
  pinMode(MOTOR_ISENSE_PIN, INPUT);
  
  // Set initial state - sleep mode
  safeDigitalWrite(MOTOR_nSLEEP_PIN, LOW);
  safeDigitalWrite(MOTOR_EN_IN1_PIN, LOW);
  safeDigitalWrite(MOTOR_PH_IN2_PIN, LOW);
  
  // Set PMODE for Phase/Enable control (Low level)
  safeDigitalWrite(MOTOR_PMODE_PIN, LOW);
  
  safeDelay(100); // Allow time for initialization
  
  motorInitialized = true;
  currentMotorState = MOTOR_SLEEP;
  Serial.println("Motor controller initialized successfully");
  
  // Wake up from sleep mode
  motorWake();
  
  // Set initial configuration using setMotorConfig
  MotorConfig defaultConfig;
  defaultConfig.maxCurrent = 1.5;  // Default 1.5A
  defaultConfig.vrefVoltage = 1.0;
  defaultConfig.currentMode = CURRENT_DISABLED;
  setMotorConfig(defaultConfig);
  
  initInProgress = false;
}

bool setMotorState(MotorState state) {
  if (!motorInitialized) {
    Serial.println("Error: Motor controller not initialized");
    return false;
  }
  
  // Check for faults before changing state
  if (checkMotorFault()) {
    Serial.println("Cannot change state: Motor fault detected");
    return false;
  }
  
  MotorState previousState = currentMotorState;
  
  switch (state) {
    case MOTOR_SLEEP:
      motorSleep();
      break;
    case MOTOR_STOP:
      motorStop();
      break;
    case MOTOR_FORWARD:
      motorForward();
      break;
    case MOTOR_REVERSE:
      motorReverse();
      break;
    default:
      Serial.println("Error: Invalid motor state");
      return false;
  }
  
  currentMotorState = state;
  return true;
}

void motorForward(void) {
  if (!motorInitialized) return;
  
  safeDigitalWrite(MOTOR_EN_IN1_PIN, HIGH);
  safeDigitalWrite(MOTOR_PH_IN2_PIN, HIGH);
  
  Serial.println("Motor: Forward");
}

void motorReverse(void) {
  if (!motorInitialized) return;
  
  safeDigitalWrite(MOTOR_EN_IN1_PIN, HIGH);
  safeDigitalWrite(MOTOR_PH_IN2_PIN, LOW);
  
  Serial.println("Motor: Reverse");
}

void motorStop(void) {
  if (!motorInitialized) return;
  
  safeDigitalWrite(MOTOR_EN_IN1_PIN, LOW);
  safeDigitalWrite(MOTOR_PH_IN2_PIN, LOW);
  
  Serial.println("Motor: Stop (Coast)");
}

void motorSleep(void) {
  if (!motorInitialized) return;
  
  safeDigitalWrite(MOTOR_nSLEEP_PIN, LOW);
  safeDigitalWrite(MOTOR_EN_IN1_PIN, LOW);
  safeDigitalWrite(MOTOR_PH_IN2_PIN, LOW);
  
  Serial.println("Motor: Sleep mode");
}

void motorWake(void) {
  if (!motorInitialized) return;
  
  safeDigitalWrite(MOTOR_nSLEEP_PIN, HIGH);
  
  // Wait for wake-up time (tWAKE = 1.5ms max)
  safeDelay(2);
  
  Serial.println("Motor: Wake up");
}

void setCurrentLimit(float currentAmps) {
  if (!motorInitialized) return;
  
  // Clamp to maximum safe current (70% of absolute max for thermal safety)
  if (currentAmps > 2.45) currentAmps = 2.45;
  if (currentAmps < 0.1) currentAmps = 0.1;
  
  // Calculate VREF voltage
  float vrefVoltage = currentAmps * 1.5; // Based on 1.5kΩ RISENSE
  
  // Convert to DAC value (0-255 for 0-3.3V)
  // Using true DAC on GPIO25 instead of PWM for better precision
  int dacValue = (int)(vrefVoltage * 255.0 / 3.3);
  dacValue = constrain(dacValue, 0, 255);
  
  // Set VREF using true DAC on GPIO25 (replaced analogWrite with dacWrite)
  dacWrite(MOTOR_VREF_PIN, dacValue);
  
  currentConfig.maxCurrent = currentAmps;
  currentConfig.vrefVoltage = vrefVoltage;
  
  Serial.print("Current limit set to: ");
  Serial.print(currentAmps);
  Serial.println("A");
}

float readMotorCurrent(void) {
  if (!motorInitialized) return 0.0;
  
  // Read ISENSE voltage
  int senseValue = analogRead(MOTOR_ISENSE_PIN);
  float senseVoltage = senseValue * 3.3 / 4095.0;
  
  // Calculate current: I = V / (R_ISENSE * A(ISENSE))
  // R_ISENSE = 1.5kΩ, A(ISENSE) = 1000μA/A = 0.001A/A
  float current = senseVoltage / (1500.0 * 0.001);
  
  return current;
}

bool checkMotorFault(void) {
  if (!motorInitialized) return true;
  
  bool faultDetected = !digitalRead(MOTOR_nFAULT_PIN);
  
  if (faultDetected) {
    Serial.println("MOTOR FAULT DETECTED!");
    
    // Read current for debugging
    float current = readMotorCurrent();
    Serial.print("Current at fault: ");
    Serial.print(current);
    Serial.println("A");
    
    // Auto-recovery attempt in certain conditions
    if (current > currentConfig.maxCurrent * 1.2) {
      Serial.println("Attempting auto-recovery...");
      motorSleep();
      safeDelay(100);
      if (currentMotorState == MOTOR_FORWARD || currentMotorState == MOTOR_REVERSE) {
        setMotorState(currentMotorState);
      }
    }
  }
  
  //return faultDetected;
  return 0;
}

void getMotorStatus(char* buffer, int bufferSize) {
  if (!buffer || bufferSize < 50) return;
  
  int pos = 0;
  pos += snprintf(buffer + pos, bufferSize - pos, "Motor Status:\n");
  pos += snprintf(buffer + pos, bufferSize - pos, "State: ");
  
  switch (currentMotorState) {
    case MOTOR_SLEEP: 
      pos += snprintf(buffer + pos, bufferSize - pos, "Sleep"); 
      break;
    case MOTOR_STOP: 
      pos += snprintf(buffer + pos, bufferSize - pos, "Stop"); 
      break;
    case MOTOR_FORWARD: 
      pos += snprintf(buffer + pos, bufferSize - pos, "Forward"); 
      break;
    case MOTOR_REVERSE: 
      pos += snprintf(buffer + pos, bufferSize - pos, "Reverse"); 
      break;
    default: 
      pos += snprintf(buffer + pos, bufferSize - pos, "Unknown"); 
      break;
  }
  
  float current = readMotorCurrent();
  pos += snprintf(buffer + pos, bufferSize - pos, "\nCurrent: %.3fA\n", current);
  pos += snprintf(buffer + pos, bufferSize - pos, "Fault: %s\n", checkMotorFault() ? "YES" : "NO");
  pos += snprintf(buffer + pos, bufferSize - pos, "Current Limit: %.2fA\n", currentConfig.maxCurrent);
  pos += snprintf(buffer + pos, bufferSize - pos, "Initialized: %s\n", motorInitialized ? "YES" : "NO");
}

void configureCurrentMode(CurrentMode mode) {
  if (!motorInitialized) return;
  
  changeCurrentMode(mode);
}

void setMotorConfig(MotorConfig config) {
  if (!motorInitialized) return;
  
  currentConfig = config;
  
  // Apply current limit
  if (config.maxCurrent > 0) {
    setCurrentLimit(config.maxCurrent);
  }
  
  // Configure current mode
  if (config.currentMode >= 0) {
    configureCurrentMode(config.currentMode);
  }
  
  Serial.println("Motor configuration updated");
}

MotorState getMotorState(void) {
  return currentMotorState;
}

bool isMotorInitialized(void) {
  return motorInitialized;
}

void demoMotorControl(void) {
  Serial.println("\n=== MOTOR CONTROL DEMO ===");
  
  // Initialize if not already done
  if (!motorInitialized) {
    initializeMotorController();
    safeDelay(1000);
  }
  
  // Test sequence
  Serial.println("1. Starting motor forward...");
  if (setMotorState(MOTOR_FORWARD)) {
    safeDelay(2000);
    
    Serial.print("Current: "); 
    Serial.print(readMotorCurrent(), 3);
    Serial.println("A");
    safeDelay(1000);
  }
  
  Serial.println("2. Stopping motor...");
  setMotorState(MOTOR_STOP);
  safeDelay(1000);
  
  Serial.println("3. Starting motor reverse...");
  if (setMotorState(MOTOR_REVERSE)) {
    safeDelay(2000);
    
    Serial.print("Current: "); 
    Serial.print(readMotorCurrent(), 3);
    Serial.println("A");
    safeDelay(1000);
  }
  
  Serial.println("4. Final stop...");
  setMotorState(MOTOR_STOP);
  safeDelay(1000);
  
  // Print final status
  char status[200];
  getMotorStatus(status, sizeof(status));
  Serial.println(status);
  Serial.println("\n=== DEMO COMPLETE ===\n");
}

void resetMotorController(void) {
  Serial.println("Resetting motor controller...");
  motorInitialized = false;
  currentMotorState = MOTOR_SLEEP;
  initializeMotorController();
}

void emergencyStop(void) {
  Serial.println("EMERGENCY STOP!");
  motorSleep();
  currentMotorState = MOTOR_SLEEP;
}

//==============================================================================
// Private Function Implementations
//==============================================================================

static void safeDigitalWrite(int pin, int value) {
  if (pin >= 0 && pin < NUM_DIGITAL_PINS) {
    digitalWrite(pin, value);
  } else {
    Serial.print("Error: Invalid pin ");
    Serial.println(pin);
  }
}

static void safeDelay(unsigned long ms) {
  // Add fault checking during delays
  unsigned long startTime = millis();
  while (millis() - startTime < ms) {
    if (checkMotorFault()) {
      Serial.println("Fault detected during delay - aborting");
      break;
    }
    delay(10);
  }
}

static void changeCurrentMode(CurrentMode targetMode) {
  // Sleep first to change mode safely
  motorSleep();
  delayMicroseconds(100); // tSLEEP requirement
  
  // Configure IMODE pin based on mode
  switch (targetMode) {
    case CURRENT_DISABLED:
      // Current control disabled
      safeDigitalWrite(MOTOR_IMODE_PIN, LOW);
      break;
      
    case CURRENT_CONSTANT:
      // Set 20kΩ pull-down for constant current PWM
      analogWrite(MOTOR_IMODE_PIN, 128); // Middle value for internal divider
      break;
      
    case CURRENT_FIXED_OFF:
      // Set GND for fixed off time control
      safeDigitalWrite(MOTOR_IMODE_PIN, LOW);
      break;
  }
  
  // Wake up with new settings
  motorWake();
  delayMicroseconds(1500); // tWAKE requirement
  
  currentConfig.currentMode = targetMode;
  
  Serial.print("Current mode set to: ");
  switch (targetMode) {
    case CURRENT_DISABLED: Serial.println("DISABLED"); break;
    case CURRENT_CONSTANT: Serial.println("CONSTANT"); break;
    case CURRENT_FIXED_OFF: Serial.println("FIXED_OFF"); break;
  }
}