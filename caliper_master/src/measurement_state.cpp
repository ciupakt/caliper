#include "measurement_state.h"
#include <string.h>

MeasurementState::MeasurementState()
    : lastValue(0.0f), ready(false), measurementInProgress(false)
{
    // Inicjalizacja buforów tekstowych
    strncpy(lastMeasurement, "Brak pomiaru", MEASUREMENT_BUFFER_SIZE - 1);
    lastMeasurement[MEASUREMENT_BUFFER_SIZE - 1] = '\0';
    
    strncpy(lastBatteryVoltage, "Brak danych", BATTERY_BUFFER_SIZE - 1);
    lastBatteryVoltage[BATTERY_BUFFER_SIZE - 1] = '\0';
}

void MeasurementState::setMeasurement(float value)
{
    lastValue = value;
    snprintf(lastMeasurement, MEASUREMENT_BUFFER_SIZE, "%.3f mm", value);
}

void MeasurementState::setBatteryVoltage(float voltage)
{
    snprintf(lastBatteryVoltage, BATTERY_BUFFER_SIZE, "%.3f V", voltage);
}

void MeasurementState::setMeasurementMessage(const char *message)
{
    if (message != nullptr)
    {
        strncpy(lastMeasurement, message, MEASUREMENT_BUFFER_SIZE - 1);
        lastMeasurement[MEASUREMENT_BUFFER_SIZE - 1] = '\0';
    }
}

void MeasurementState::setReady(bool isReady)
{
    ready = isReady;
}

const char *MeasurementState::getMeasurement() const
{
    return lastMeasurement;
}

const char *MeasurementState::getBatteryVoltage() const
{
    return lastBatteryVoltage;
}

float MeasurementState::getValue() const
{
    return lastValue;
}

bool MeasurementState::isReady() const
{
    return ready;
}

bool MeasurementState::isMeasurementInProgress() const
{
    return measurementInProgress;
}

void MeasurementState::setMeasurementInProgress(bool inProgress)
{
    measurementInProgress = inProgress;
}

void MeasurementState::reset()
{
    lastValue = 0.0f;
    ready = false;
    measurementInProgress = false;
    
    strncpy(lastMeasurement, "Brak pomiaru", MEASUREMENT_BUFFER_SIZE - 1);
    lastMeasurement[MEASUREMENT_BUFFER_SIZE - 1] = '\0';
    
    strncpy(lastBatteryVoltage, "Brak danych", BATTERY_BUFFER_SIZE - 1);
    lastBatteryVoltage[BATTERY_BUFFER_SIZE - 1] = '\0';
}
