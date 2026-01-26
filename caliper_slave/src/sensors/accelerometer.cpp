/**
 * @file accelerometer.cpp
 * @brief ADXL345 accelerometer sensor implementation for ESP32
 * @author System Generated
 * @date 2025-12-27
 * @version 2.0
 *
 * @version 2.0 - Integrated comprehensive error code system
 */

#include "accelerometer.h"

#include <MacroDebugger.h>
#include <error_handler.h>

#define ADXL345_I2CADDR 0x53 // 0x1D if SDO = HIGH

bool AccelerometerInterface::begin()
{
    //TwoWire wirePort = Wire;
    //wirePort.begin();

    myAcc = ADXL345_WE(ADXL345_I2CADDR);
    
    if (!myAcc.init())
    {
        RECORD_ERROR(ERR_ACCEL_INIT_FAILED, "ADXL345 not connected at I2C address 0x%02X", ADXL345_I2CADDR);
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
