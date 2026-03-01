/**
 * @file accelerometer.h
 * @brief IIS328DQ accelerometer sensor interface for ESP32
 * @author System Generated
 * @date 2025-12-27
 * @version 3.0
 *
 * @details
 * Provides interface for reading angle measurements from IIS328DQ
 * accelerometer via I2C communication.
 *
 * @version 2.0 - Integrated comprehensive error code system
 * @version 3.0 - Migrated from ADXL345 to IIS328DQ
 */

#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include <Arduino.h>
#include <Wire.h>
#include "../config.h"
#include <shared_common.h>
#include <error_handler.h>

/**
 * @brief Simple 3-axis float structure for angle data
 */
struct AngleData {
    float x;
    float y;
    float z;
};

/**
 * @brief IIS328DQ accelerometer driver class
 * 
 * Simple I2C driver for ST IIS328DQ 3-axis accelerometer.
 * Provides tilt angle measurements in degrees.
 */
class AccelerometerInterface {
private:
    // IIS328DQ I2C address (SA0 = VDD)
    static constexpr uint8_t IIS328DQ_I2CADDR = 0x18;
    
    // IIS328DQ WHO_AM_I value
    static constexpr uint8_t IIS328DQ_WHO_AM_I = 0x32;
    
    // Register addresses
    static constexpr uint8_t REG_WHO_AM_I = 0x0F;
    static constexpr uint8_t REG_CTRL_REG1 = 0x20;
    static constexpr uint8_t REG_CTRL_REG4 = 0x23;
    static constexpr uint8_t REG_OUT_X_L = 0x28;
    static constexpr uint8_t REG_OUT_X_H = 0x29;
    
    // Sensitivity for ±2g range: 0.98 mg/LSB
    static constexpr float SENSIVITY_MG_PER_LSB = 0.98f;
    
    // Angle data - initialized to zero for safety
    AngleData angle = {0.0f, 0.0f, 0.0f};
    
    /**
     * @brief Read a single register
     * @param reg Register address
     * @return Register value
     */
    uint8_t readRegister(uint8_t reg);
    
    /**
     * @brief Write to a single register
     * @param reg Register address
     * @param value Value to write
     */
    void writeRegister(uint8_t reg, uint8_t value);
    
public:
    /**
     * @brief Initialize accelerometer
     * @details Initializes I2C communication and configures IIS328DQ
     *
     * Possible errors:
     * - ERR_ACCEL_INIT_FAILED: IIS328DQ initialization failed
     * - ERR_ACCEL_I2C_ERROR: I2C communication error
     *
     * @return true if initialization successful, false otherwise
     */
    bool begin();
    
    /**
     * @brief Read current angle values
     * @details Updates internal angle values from accelerometer
     *
     * Possible errors:
     * - ERR_ACCEL_READ_FAILED: Read operation failed
     */
    void update();
    
    /**
     * @brief Get X-axis angle (Roll)
     * @return Angle in degrees
     */
    float getAngleX() const { return angle.x; }
    
    /**
     * @brief Get Y-axis angle (Pitch)
     * @return Angle in degrees
     */
    float getAngleY() const { return angle.y; }
    
    /**
     * @brief Get Z-axis angle (inclination from vertical)
     * @return Angle in degrees
     */
    float getAngleZ() const { return angle.z; }
};

#endif // ACCELEROMETER_H
