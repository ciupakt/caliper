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
    int raw = analogRead(BATTERY_VOLTAGE_PIN);

    uint16_t voltage_adc_mV = (uint16_t)((raw * ADC_REFERENCE_VOLTAGE_MV) / ADC_RESOLUTION);
    float voltage_mV = (float)((uint32_t)voltage_adc_mV * (BATTERY_DIVIDER_R1 + BATTERY_DIVIDER_R2) / BATTERY_DIVIDER_R2);

    if (!filterInitialized)
    {
        filteredVoltage_mV = voltage_mV;
        filterInitialized = true;
    }
    else
    {
        filteredVoltage_mV = filteredVoltage_mV + BATTERY_FILTER_ALPHA * (voltage_mV - filteredVoltage_mV);
    }

    return (uint16_t)filteredVoltage_mV;
}
