/**
 * @file accelerometer.h
 * @brief ADXL345 accelerometer sensor interface for ESP32
 * @author System Generated
 * @date 2025-12-27
 * @version 2.0
 *
 * @details
 * Provides interface for reading angle measurements from ADXL345
 * accelerometer via I2C communication.
 *
 * @version 2.0 - Integrated comprehensive error code system
 */

#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include <Arduino.h>
#include <ADXL345_WE.h>
#include "../config.h"
#include <shared_common.h>
#include <error_handler.h>

class AccelerometerInterface {
private:
    ADXL345_WE myAcc;
    xyzFloat angle;
    
public:
    /**
     * @brief Initialize accelerometer
     * @details Initializes I2C communication and configures ADXL345
     *
     * Possible errors:
     * - ERR_ACCEL_INIT_FAILED: ADXL345 initialization failed
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
     * @brief Get X-axis angle
     * @return Angle in degrees
     */
    float getAngleX() const { return angle.x; }
    
    /**
     * @brief Get Y-axis angle
     * @return Angle in degrees
     */
    float getAngleY() const { return angle.y; }
    
    /**
     * @brief Get Z-axis angle
     * @return Angle in degrees
     */
    float getAngleZ() const { return angle.z; }
};

#endif // ACCELEROMETER_H
