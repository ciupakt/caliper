/**
 * @file accelerometer.cpp
 * @brief ADXL345 accelerometer sensor implementation for ESP32
 * @author System Generated
 * @date 2025-12-27
 * @version 1.0
 */

#include "accelerometer.h"

#include <MacroDebugger.h>

#define ADXL345_I2CADDR 0x53 // 0x1D if SDO = HIGH

bool AccelerometerInterface::begin()
{
    myAcc = ADXL345_WE(ADXL345_I2CADDR);
    
    if (!myAcc.init())
    {
        DEBUG_E("ADXL345 not connected!");
        return false;
    }

    myAcc.setDataRate(ADXL345_DATA_RATE_50);
    myAcc.setRange(ADXL345_RANGE_2G);
    
    DEBUG_I("ADXL345 initialized successfully");
    return true;
}

void AccelerometerInterface::update()
{
    myAcc.getAngles(&angle);
}
