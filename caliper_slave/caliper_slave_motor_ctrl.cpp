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
  
  // ISET pin should be connected to ground via resistor
  // No pin configuration needed for ISET
  
  // Configure input pins
  pinMode(MOTOR_VISEN_PIN, INPUT);
  
  // Set initial state
  safeDigitalWrite(MOTOR_IN1_PIN, LOW);
  safeDigitalWrite(MOTOR_IN2_PIN, LOW);
  
  safeDelay(100); // Allow time for initialization
  
  motorInitialized = true;
  currentMotorState = MOTOR_STOP;
  Serial.println("Motor controller initialized successfully");
  
  // Set initial configuration using setMotorConfig
  MotorConfig defaultConfig;
#ifdef POLOLU_MP6550_CARRIER
  defaultConfig.maxCurrent = 2.5;  // Default 2.5A for Pololu
  defaultConfig.isetResistance = 0.2; // 0.2kΩ for 2.5A (requires hardware mod)
#else
  defaultConfig.maxCurrent = 1.0;  // Default 1.0A
  defaultConfig.isetResistance = 0.5; // 0.5kΩ for 1A
#endif
  defaultConfig.currentMode = CURRENT_AUTO;
  defaultConfig.motorSpeed = 0;
  defaultConfig.pwmEnabled = false;
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
      motorStop(); // Just stop the motor instead of sleep
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
    case MOTOR_BRAKE:
      motorBrake();
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

void motorBrake(void) {
  if (!motorInitialized) return;
  
  // MP6550GG-Z: IN1=HIGH, IN2=HIGH for Brake
  safeDigitalWrite(MOTOR_IN1_PIN, HIGH);
  safeDigitalWrite(MOTOR_IN2_PIN, HIGH);
  
  Serial.println("Motor: Brake");
}

void motorStop(void) {
  if (!motorInitialized) return;
  
  // MP6550GG-Z: IN1=LOW, IN2=LOW for Coast
  safeDigitalWrite(MOTOR_IN1_PIN, LOW);
  safeDigitalWrite(MOTOR_IN2_PIN, LOW);
  
  Serial.println("Motor: Stop (Coast)");
}

void setCurrentLimit(float currentAmps) {
  if (!motorInitialized) return;
  
  // Clamp to maximum safe current (2.5A for Pololu MP6550 Carrier)
  if (currentAmps > 2.5) currentAmps = 2.5;
  if (currentAmps < 0.1) currentAmps = 0.1;
  
  // Calculate required ISET resistance according to MP6550GG-Z datasheet
  // I_LIMIT = 0.5V / R_ISET  =>  R_ISET = 0.5V / I_LIMIT
  float resistance = calculateIsetResistance(currentAmps);
  
  // NOTE: ISET pin requires a physical resistor to ground.
  // The previous DAC implementation was incorrect - ISET is a current-sensing
  // input, not a voltage-controlled input.
  // 
  // For proper operation, connect a resistor between ISET pin and ground:
  // - 0.25kΩ (250Ω) for 2A current limit
  // - 0.5kΩ (500Ω) for 1A current limit  
  // - 1kΩ for 0.5A current limit
  // - 2kΩ for 0.25A current limit
  //
  // This function now only calculates and stores the required resistance value.
  // Physical resistor must be changed manually or with digital potentiometer.
  
  currentConfig.maxCurrent = currentAmps;
  currentConfig.isetResistance = resistance;
  
  Serial.print("Current limit set to: ");
  Serial.print(currentAmps);
  Serial.print("A (requires R_ISET = ");
  Serial.print(resistance, 3);
  Serial.println("kΩ to ground)");
  
  Serial.println("NOTE: Physical resistor change required for current limit adjustment");
}

float readMotorCurrent(void) {
  if (!motorInitialized) return 0.0;
  
  // Read VISEN voltage
  // MP6550GG-Z datasheet: VISEN = I_OUT * R_ISET * 100μA/A
  // Where: I_OUT = motor current, R_ISET = resistance to ground, 100μA/A = 0.0001
  // Therefore: I_OUT = VISEN / (R_ISET * 0.0001)
  
  int senseValue = analogRead(MOTOR_VISEN_PIN);
  float senseVoltage = senseValue * 3.3 / 4095.0; // Assuming 3.3V ADC reference
  
  // Calculate current using proper MP6550GG-Z formula
  float current = senseVoltage / (currentConfig.isetResistance * 0.0001);
  
  return current;
}

bool checkMotorFault(void) {
  if (!motorInitialized) return true;
  
  bool faultDetected = false;
  float current = readMotorCurrent();
  unsigned long currentTime = millis();
  
  // Check for overcurrent (integrate with MP6550 built-in OCP)
  // MP6550 has built-in OCP at 4-6A, but we monitor for software limits
  if (current > currentConfig.maxCurrent * 1.2) { // Reduced threshold for better protection
    if (!overcurrentDetected) {
      Serial.println("OVERCURRENT DETECTED!");
      Serial.print("Current: ");
      Serial.print(current, 3);
      Serial.print("A (limit: ");
      Serial.print(currentConfig.maxCurrent, 2);
      Serial.println("A)");
      overcurrentDetected = true;
      faultDetected = true;
    }
  } else {
    overcurrentDetected = false;
  }
  
  // Check for thermal shutdown (improved detection)
  if ((currentMotorState == MOTOR_FORWARD || currentMotorState == MOTOR_REVERSE) && 
      currentTime - faultCheckTime > 100) {
    
    // TSD detection: current drops to near zero while motor should be running
    if (previousCurrent > 0.2 && current < 0.05) {
      Serial.println("THERMAL SHUTDOWN DETECTED!");
      Serial.print("Current drop: ");
      Serial.print(previousCurrent, 3);
      Serial.print("A → ");
      Serial.print(current, 3);
      Serial.println("A");
      faultDetected = true;
    }
    
    previousCurrent = current;
    faultCheckTime = currentTime;
  }
  
  // Auto-recovery for overcurrent (improved)
  if (faultDetected && overcurrentDetected) {
    Serial.println("Attempting auto-recovery from overcurrent...");
    motorStop();
    delay(100); // Wait for fault to clear
    
    // Restore previous state if it was running
    if (currentMotorState == MOTOR_FORWARD || currentMotorState == MOTOR_REVERSE) {
      if (currentConfig.pwmEnabled && currentConfig.motorSpeed > 0) {
        setMotorSpeed(currentConfig.motorSpeed, currentMotorState);
      } else {
        if (currentMotorState == MOTOR_FORWARD) {
          motorForward();
        } else if (currentMotorState == MOTOR_REVERSE) {
          motorReverse();
        }
      }
    }
  }
  
  return faultDetected;
}

void getMotorStatus(char* buffer, int bufferSize) {
  if (!buffer || bufferSize < 200) return; // Increased buffer size requirement

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
    case MOTOR_BRAKE:
      pos += snprintf(buffer + pos, bufferSize - pos, "Brake");
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
  pos += snprintf(buffer + pos, bufferSize - pos, "PWM Enabled: %s\n", currentConfig.pwmEnabled ? "YES" : "NO");
  if (currentConfig.pwmEnabled) {
    pos += snprintf(buffer + pos, bufferSize - pos, "Motor Speed: %d/255 (%d%%)\n", 
                 currentConfig.motorSpeed, (currentConfig.motorSpeed * 100) / 255);
  }
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
  motorStop();
  currentMotorState = MOTOR_STOP;
}

//==============================================================================
// Private Function Implementations
//==============================================================================

static void safeDigitalWrite(int pin, int value) {
  // NUM_DIGITAL_PINS is an Arduino core constant, typically 40 for ESP32
  // This check ensures the pin number is valid before attempting to write.
  // It's a basic safeguard, actual pin mapping should be correct in header.
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
    delay(10); // Check every 10ms
  }
}

static float calculateIsetResistance(float currentAmps) {
  // MP6550GG-Z: I_LIMIT = 0.5V / R_ISET
  // Therefore: R_ISET = 0.5V / I_LIMIT
  float resistance = 0.5 / currentAmps; // Result in kΩ

  // Clamp to reasonable range (0.25kΩ to 5kΩ)
  if (resistance < 0.25) resistance = 0.25; // Max current ~2A
  if (resistance > 5.0) resistance = 5.0;   // Min current ~0.1A

  return resistance;
}

static void setIsetResistance(float resistance) {
  // DEPRECATED: This function is no longer needed
  // ISET pin requires a physical resistor to ground, not DAC control
  // See setCurrentLimit() for proper implementation
  Serial.println("setIsetResistance() deprecated - use physical resistor");
}

void setMotorSpeed(uint8_t speed, MotorState direction) {
  if (!motorInitialized) return;
  
  // Clamp speed to valid range
  speed = constrain(speed, 0, 255);
  currentConfig.motorSpeed = speed;
  
  // Configure PWM for motor control using analogWrite (ESP32 compatible)
  // MP6550GG-Z supports PWM up to 100kHz
  // Using 5kHz for good balance of performance and noise
  
  if (speed == 0) {
    // Stop motor
    motorStop();
    return;
  }
  
  // Setup PWM channels for motor control
  // ESP32: analogWrite uses LEDC automatically
  
  switch (direction) {
    case MOTOR_FORWARD:
      // Forward: IN1=HIGH, IN2=PWM (Table 2 in datasheet)
      analogWrite(MOTOR_IN1_PIN, 255);  // IN1 = HIGH
      analogWrite(MOTOR_IN2_PIN, 255 - speed); // IN2 = PWM (inverted for speed control)
      break;
      
    case MOTOR_REVERSE:
      // Reverse: IN1=PWM, IN2=HIGH (Table 2 in datasheet)
      analogWrite(MOTOR_IN1_PIN, 255 - speed); // IN1 = PWM (inverted for speed control)
      analogWrite(MOTOR_IN2_PIN, 255);  // IN2 = HIGH
      break;
      
    case MOTOR_BRAKE:
      // Brake: Both HIGH with PWM
      analogWrite(MOTOR_IN1_PIN, speed);
      analogWrite(MOTOR_IN2_PIN, speed);
      break;
      
    default:
      Serial.println("Error: Invalid direction for PWM control");
      motorStop();
      return;
  }
  
  currentMotorState = direction;
  
  Serial.print("Motor speed set to: ");
  Serial.print(speed);
  Serial.print("/255 (");
  Serial.print((speed * 100) / 255);
  Serial.print("%) - Direction: ");
  Serial.println(direction == MOTOR_FORWARD ? "Forward" : "Reverse");
}

uint8_t getMotorSpeed(void) {
  return currentConfig.motorSpeed;
}

void testMotorController(void) {
  Serial.println("\n=== MP6550GG-Z COMPLIANCE TEST ===");
  
  if (!motorInitialized) {
    Serial.println("ERROR: Motor controller not initialized!");
    return;
  }
  
  // Test 1: Pin configuration verification
  Serial.println("1. Testing pin configuration...");
  Serial.print("   IN1 Pin: "); Serial.println(MOTOR_IN1_PIN);
  Serial.print("   IN2 Pin: "); Serial.println(MOTOR_IN2_PIN);
  Serial.print("   ISET Pin: "); Serial.println(MOTOR_ISET_PIN);
  Serial.print("   VISEN Pin: "); Serial.println(MOTOR_VISEN_PIN);
  
  // Test 2: Current sensing
  Serial.println("2. Testing current sensing...");
  float motorCurrent = readMotorCurrent();
  Serial.print("   Motor Current: "); Serial.print(motorCurrent, 3); Serial.println("A");
  Serial.print("   Current Limit: "); Serial.print(currentConfig.maxCurrent, 2); Serial.println("A");
  Serial.print("   ISET Resistance: "); Serial.print(currentConfig.isetResistance, 3); Serial.println("kΩ");
  
  // Test 3: PWM functionality
  Serial.println("3. Testing PWM speed control...");
  setMotorSpeed(128, MOTOR_FORWARD); // 50% speed
  delay(500);
  Serial.print("   PWM Forward 50% - Current: "); Serial.print(readMotorCurrent(), 3); Serial.println("A");
  
  setMotorSpeed(255, MOTOR_FORWARD); // 100% speed
  delay(500);
  Serial.print("   PWM Forward 100% - Current: "); Serial.print(readMotorCurrent(), 3); Serial.println("A");
  
  setMotorSpeed(0, MOTOR_STOP); // Stop
  delay(500);
  
  // Test 4: Fault detection
  Serial.println("4. Testing fault detection...");
  bool faultStatus = checkMotorFault();
  Serial.print("   Fault Status: "); Serial.println(faultStatus ? "DETECTED" : "NONE");
  
  // Test 5: Complete status report
  Serial.println("5. Complete status report:");
  char statusBuffer[500];
  getMotorStatus(statusBuffer, sizeof(statusBuffer));
  Serial.println(statusBuffer);
  
  Serial.println("=== COMPLIANCE TEST COMPLETE ===\n");
}