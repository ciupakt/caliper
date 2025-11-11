/**
 * @file    caliper_slave_motor_ctrl.cpp
 * @brief   MP6550GG-Z DC Motor Controller Implementation for ESP32
 * @author  Generated from caliper_slave_motor_ctrl.ino
 * @date    2025-11-11
 * @version 2.0
 * 
 * @details
 * Implementation file for MP6550GG-Z single H-Bridge DC motor driver.
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

/** LDO regulator state */
static bool ldoEnabled = true;

/** Fault detection variables */
static float previousCurrent = 0.0;
static unsigned long faultCheckTime = 0;
static bool overcurrentDetected = false;

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
 * @brief Internal function to calculate ISET resistance from current
 * @param currentAmps Desired current limit
 * @return Required ISET resistance in Ohms
 */
static float calculateIsetResistance(float currentAmps);

/**
 * @brief Internal function to set ISET using DAC
 * @param resistance Desired resistance in Ohms
 */
static void setIsetResistance(float resistance);

//==============================================================================
// Public Function Implementations
//==============================================================================

void initializeMotorController(void) {
  // Prevent recursive initialization
  if (initInProgress || motorInitialized) {
    return;
  }
  
  initInProgress = true;
  Serial.println("Initializing MP6550GG-Z Motor Controller...");
  
  // Configure pins as outputs/inputs
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
  pinMode(MOTOR_ISET_PIN, OUTPUT);
  pinMode(MOTOR_nSLEEP_HB_PIN, OUTPUT);
  pinMode(MOTOR_nSLEEP_LDO_PIN, OUTPUT);
  pinMode(MOTOR_VISEN_PIN, INPUT);
  
  // Set initial state - sleep mode
  safeDigitalWrite(MOTOR_nSLEEP_HB_PIN, LOW);
  safeDigitalWrite(MOTOR_nSLEEP_LDO_PIN, LOW);
  safeDigitalWrite(MOTOR_IN1_PIN, LOW);
  safeDigitalWrite(MOTOR_IN2_PIN, LOW);
  
  // Initialize DAC for ISET control
  dacWrite(MOTOR_ISET_PIN, 0); // Start with minimum current
  
  safeDelay(100); // Allow time for initialization
  
  motorInitialized = true;
  currentMotorState = MOTOR_SLEEP;
  ldoEnabled = false;
  Serial.println("Motor controller initialized successfully");
  
  // Wake up from sleep mode
  motorWake();
  
  // Set initial configuration using setMotorConfig
  MotorConfig defaultConfig;
  defaultConfig.maxCurrent = 1.0;  // Default 1.0A
  defaultConfig.isetResistance = 0.5; // 0.5kΩ for 1A
  defaultConfig.currentMode = CURRENT_AUTO;
  defaultConfig.ldoEnabled = true;
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
  
  // MP6550GG-Z: IN1=HIGH, IN2=LOW for Forward
  safeDigitalWrite(MOTOR_IN1_PIN, HIGH);
  safeDigitalWrite(MOTOR_IN2_PIN, LOW);
  
  Serial.println("Motor: Forward");
}

void motorReverse(void) {
  if (!motorInitialized) return;
  
  // MP6550GG-Z: IN1=LOW, IN2=HIGH for Reverse
  safeDigitalWrite(MOTOR_IN1_PIN, LOW);
  safeDigitalWrite(MOTOR_IN2_PIN, HIGH);
  
  Serial.println("Motor: Reverse");
}

void motorStop(void) {
  if (!motorInitialized) return;
  
  // MP6550GG-Z: IN1=LOW, IN2=LOW for Coast
  safeDigitalWrite(MOTOR_IN1_PIN, LOW);
  safeDigitalWrite(MOTOR_IN2_PIN, LOW);
  
  Serial.println("Motor: Stop (Coast)");
}

void motorSleep(void) {
  if (!motorInitialized) return;
  
  // Put both H-bridge and LDO to sleep
  safeDigitalWrite(MOTOR_nSLEEP_HB_PIN, LOW);
  safeDigitalWrite(MOTOR_nSLEEP_LDO_PIN, LOW);
  safeDigitalWrite(MOTOR_IN1_PIN, LOW);
  safeDigitalWrite(MOTOR_IN2_PIN, LOW);
  
  ldoEnabled = false;
  currentMotorState = MOTOR_SLEEP;
  
  Serial.println("Motor: Sleep mode");
}

void motorWake(void) {
  if (!motorInitialized) return;
  
  // Wake up LDO first, then H-bridge
  safeDigitalWrite(MOTOR_nSLEEP_LDO_PIN, HIGH);
  safeDelay(1); // Small delay for LDO stabilization
  
  safeDigitalWrite(MOTOR_nSLEEP_HB_PIN, HIGH);
  
  // Wait for wake-up time (tWAKE = 200μs max)
  delayMicroseconds(200);
  
  ldoEnabled = true;
  
  Serial.println("Motor: Wake up");
}

void setCurrentLimit(float currentAmps) {
  if (!motorInitialized) return;
  
  // Clamp to maximum safe current (2.0A for MP6550GG-Z)
  if (currentAmps > 2.0) currentAmps = 2.0;
  if (currentAmps < 0.1) currentAmps = 0.1;
  
  // Calculate required ISET resistance
  float resistance = calculateIsetResistance(currentAmps);
  
  // Set ISET using DAC
  setIsetResistance(resistance);
  
  currentConfig.maxCurrent = currentAmps;
  currentConfig.isetResistance = resistance;
  
  Serial.print("Current limit set to: ");
  Serial.print(currentAmps);
  Serial.print("A (R_ISET = ");
  Serial.print(resistance, 3);
  Serial.println("kΩ)");
}

float readMotorCurrent(void) {
  if (!motorInitialized) return 0.0;
  
  // Read VISEN voltage
  int senseValue = analogRead(MOTOR_VISEN_PIN);
  float senseVoltage = senseValue * 3.3 / 4095.0;
  
  // Calculate current based on ISET resistance
  // VISEN = I_OUT * R_ISET * (100μA/A)
  // I_OUT = VISEN / (R_ISET * 0.0001)
  float current = senseVoltage / (currentConfig.isetResistance * 0.0001);
  
  return current;
}

bool checkMotorFault(void) {
  if (!motorInitialized) return true;
  
  bool faultDetected = false;
  float current = readMotorCurrent();
  unsigned long currentTime = millis();
  
  // Check for overcurrent
  if (current > currentConfig.maxCurrent * 1.5) {
    if (!overcurrentDetected) {
      Serial.println("OVERCURRENT DETECTED!");
      overcurrentDetected = true;
      faultDetected = true;
    }
  } else {
    overcurrentDetected = false;
  }
  
  // Check for thermal shutdown (current suddenly drops to 0 while motor should be running)
  if ((currentMotorState == MOTOR_FORWARD || currentMotorState == MOTOR_REVERSE) && 
      currentTime - faultCheckTime > 100) {
    
    if (previousCurrent > 0.1 && current < 0.01) {
      Serial.println("THERMAL SHUTDOWN DETECTED!");
      faultDetected = true;
    }
    
    previousCurrent = current;
    faultCheckTime = currentTime;
  }
  
  // Auto-recovery attempt for overcurrent
  if (faultDetected && overcurrentDetected) {
    Serial.println("Attempting auto-recovery...");
    motorSleep();
    safeDelay(100);
    motorWake();
    if (currentMotorState == MOTOR_FORWARD || currentMotorState == MOTOR_REVERSE) {
      setMotorState(currentMotorState);
    }
  }
  
  return faultDetected;
}

void getMotorStatus(char* buffer, int bufferSize) {
  if (!buffer || bufferSize < 100) return;
  
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
  pos += snprintf(buffer + pos, bufferSize - pos, "ISET Resistance: %.3fkΩ\n", currentConfig.isetResistance);
  pos += snprintf(buffer + pos, bufferSize - pos, "LDO Enabled: %s\n", ldoEnabled ? "YES" : "NO");
  pos += snprintf(buffer + pos, bufferSize - pos, "Initialized: %s\n", motorInitialized ? "YES" : "NO");
}

void configureCurrentMode(CurrentMode mode) {
  if (!motorInitialized) return;
  
  currentConfig.currentMode = mode;
  
  Serial.print("Current mode set to: ");
  switch (mode) {
    case CURRENT_AUTO: Serial.println("AUTO (built-in regulation)"); break;
    case CURRENT_MANUAL: Serial.println("MANUAL (via ISET)"); break;
  }
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
  
  // Set LDO state
  setLDOState(config.ldoEnabled);
  
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
  char status[300];
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

void setLDOState(bool enabled) {
  if (!motorInitialized) return;
  
  if (enabled && !ldoEnabled) {
    safeDigitalWrite(MOTOR_nSLEEP_LDO_PIN, HIGH);
    safeDelay(1); // Allow LDO to stabilize
    ldoEnabled = true;
    Serial.println("LDO Enabled");
  } else if (!enabled && ldoEnabled) {
    safeDigitalWrite(MOTOR_nSLEEP_LDO_PIN, LOW);
    ldoEnabled = false;
    Serial.println("LDO Disabled");
  }
  
  currentConfig.ldoEnabled = enabled;
}

bool getLDOState(void) {
  return ldoEnabled;
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

static float calculateIsetResistance(float currentAmps) {
  // MP6550GG-Z: I_LIMIT = 0.5V / R_ISET
  // Therefore: R_ISET = 0.5V / I_LIMIT
  float resistance = 0.5 / currentAmps; // Result in kΩ
  
  // Clamp to reasonable range (0.25kΩ to 5kΩ)
  if (resistance < 0.25) resistance = 0.25;
  if (resistance > 5.0) resistance = 5.0;
  
  return resistance;
}

static void setIsetResistance(float resistance) {
  // Use DAC to simulate variable resistance
  // We'll use a voltage divider approach with a 1kΩ resistor
  // DAC output through 1kΩ to ISET pin creates variable resistance
  
  // Calculate DAC value (0-255 for 0-3.3V)
  // Higher DAC voltage = lower effective resistance
  // This is an approximation - real implementation might need external circuitry
  float dacVoltage = 3.3 * (1.0 - (resistance - 0.25) / 4.75);
  int dacValue = (int)(dacVoltage * 255.0 / 3.3);
  dacValue = constrain(dacValue, 0, 255);
  
  dacWrite(MOTOR_ISET_PIN, dacValue);
}