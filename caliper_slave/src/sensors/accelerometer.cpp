/**
 * @file accelerometer.cpp
 * @brief IIS328DQ accelerometer sensor implementation for ESP32
 * @author System Generated
 * @date 2025-12-27
 * @version 3.0
 *
 * @version 2.0 - Integrated comprehensive error code system
 * @version 3.0 - Migrated from ADXL345 to IIS328DQ
 */

#include "accelerometer.h"

#include <MacroDebugger.h>
#include <error_handler.h>
#include <math.h>

// Register addresses for IIS328DQ
#define IIS328DQ_WHO_AM_I_REG 0x0F
#define IIS328DQ_CTRL_REG1 0x20
#define IIS328DQ_CTRL_REG4 0x23
#define IIS328DQ_OUT_X_L 0x28

// Control register values
#define CTRL_REG1_VALUE 0x27  // PM=001 (normal mode), DR=00 (50Hz), Zen=Yen=Xen=1
#define CTRL_REG4_VALUE 0x80  // BDU=1 (block data update), FS=00 (±2g)

uint8_t AccelerometerInterface::readRegister(uint8_t reg)
{
    Wire.beginTransmission(IIS328DQ_I2CADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    //Wire.endTransmission();
    
    Wire.requestFrom((int)IIS328DQ_I2CADDR, 1);
    return Wire.read();
}

void AccelerometerInterface::writeRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(IIS328DQ_I2CADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

bool AccelerometerInterface::begin()
{
    Wire.begin(3, 46);

    // Check device ID
    uint8_t deviceId = readRegister(IIS328DQ_WHO_AM_I_REG);
    if (deviceId != IIS328DQ_WHO_AM_I)
    {
        RECORD_ERROR(ERR_ACCEL_INIT_FAILED, "IIS328DQ not connected (found ID: 0x%02X, expected: 0x%02X)", deviceId, IIS328DQ_WHO_AM_I);
        return false;
    }
    
    // Configure: Normal mode, 50Hz, all axes enabled
    writeRegister(IIS328DQ_CTRL_REG1, CTRL_REG1_VALUE);
    
    // Configure: BDU enabled, ±2g range
    writeRegister(IIS328DQ_CTRL_REG4, CTRL_REG4_VALUE);
    
    DEBUG_I("IIS328DQ initialized successfully at address 0x%02X", IIS328DQ_I2CADDR);
    return true;
}

void AccelerometerInterface::update()
{
    // Read 6 bytes starting at OUT_X_L with auto-increment (MSB=1)
    Wire.beginTransmission(IIS328DQ_I2CADDR);
    Wire.write(IIS328DQ_OUT_X_L | 0x80);  // Set MSB for auto-increment
    uint8_t error = Wire.endTransmission(false);
    
    if (error != 0)
    {
        RECORD_ERROR(ERR_ACCEL_I2C_ERROR, "IIS328DQ I2C error: %d", error);
        return;
    }
    
    Wire.requestFrom((int)IIS328DQ_I2CADDR, 6);
    
    if (Wire.available() < 6)
    {
        RECORD_ERROR(ERR_ACCEL_READ_FAILED, "IIS328DQ insufficient data available");
        return;
    }
    
    // Read acceleration data (little-endian: LOW byte first, then HIGH byte)
    int16_t rawX = (int16_t)(Wire.read() | (Wire.read() << 8));
    int16_t rawY = (int16_t)(Wire.read() | (Wire.read() << 8));
    int16_t rawZ = (int16_t)(Wire.read() | (Wire.read() << 8));
    
    // Convert to g (±2g range, 0.98 mg/LSB)
    float accX = rawX * SENSIVITY_MG_PER_LSB * 0.001f;  // Convert mg to g
    float accY = rawY * SENSIVITY_MG_PER_LSB * 0.001f;
    float accZ = rawZ * SENSIVITY_MG_PER_LSB * 0.001f;
    
    // Calculate tilt angles in degrees
    // Roll (X-axis rotation) - angle around X-axis
    angle.x = atan2(accY, accZ) * RAD_TO_DEG;
    
    // Pitch (Y-axis rotation) - angle around Y-axis
    angle.y = atan2(-accX, sqrt(accY * accY + accZ * accZ)) * RAD_TO_DEG;
    
    // Z-angle (inclination from vertical)
    float magnitude = sqrt(accX * accX + accY * accY + accZ * accZ);
    if (magnitude > 0.001f)
    {
        angle.z = acos(accZ / magnitude) * RAD_TO_DEG;
    }
    else
    {
        angle.z = 0.0f;
    }
}
