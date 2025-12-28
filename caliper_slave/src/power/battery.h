/**
 * @file battery.h
 * @brief Battery voltage monitoring interface for ESP32
 * @author System Generated
 * @date 2025-12-27
 * @version 1.0
 * 
 * @details
 * Provides interface for reading battery voltage with caching
 * and averaging for better accuracy.
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include "../config.h"

class BatteryMonitor {

public:
    /**
     * @brief Initialize battery monitor
     * @details Sets up ADC pin for voltage reading
     */
    BatteryMonitor(){}

    /**
     * @brief Force a new voltage reading
     * @return Voltage in millivolts
     * @details Bypasses cache and performs immediate measurement
     */
    uint16_t readVoltageNow();
};

#endif // BATTERY_H
