/**
 * @file battery.cpp
 * @brief Battery voltage monitoring implementation for ESP32
 * @author System Generated
 * @date 2025-12-27
 * @version 1.0
 */

#include "battery.h"


uint16_t BatteryMonitor::readVoltageNow()
{
    // Read and average multiple samples for better accuracy
    uint32_t adcSum = 0;

    for (int i = 0; i < ADC_SAMPLES; i++)
    {
        adcSum += analogRead(BATTERY_VOLTAGE_PIN);
        delay(1); // Small delay between samples
    }

    int adcAverage = adcSum / ADC_SAMPLES;

    // Convert ADC value to millivolts using constants from config.h
    uint16_t voltage_mV = (uint16_t)((adcAverage * ADC_REFERENCE_VOLTAGE_MV) / ADC_RESOLUTION);

    return voltage_mV;
}
